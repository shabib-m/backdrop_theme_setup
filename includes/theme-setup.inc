<?php

/**
 * Read a JSON setup file from the theme directory.
 *
 * @param string $theme
 *   Theme machine name.
 * @param string $relative_path
 *   File path relative to settings-content.
 *
 * @return array|false
 *   Decoded JSON data, or FALSE on failure.
 */
function theme_setup_read_json($theme, $relative_path) {
  if (!is_string($theme) || !preg_match('/^[a-z0-9_]+$/', $theme) ||
      !is_string($relative_path) ||
      !preg_match('@^[a-zA-Z0-9_/-]+\.json$@', $relative_path)) {
    return FALSE;
  }

  $base = backdrop_get_path('theme', $theme);

  if (!$base) {
    return FALSE;
  }

  $directory = realpath($base . '/settings-content');
  $file = realpath($base . '/settings-content/' . $relative_path);

  // Ensure the file is inside the settings-content directory.
  if (!$directory || !$file ||
      strpos($file, $directory . DIRECTORY_SEPARATOR) !== 0 ||
      !is_file($file) || !is_readable($file)) {
    return FALSE;
  }

  $data = backdrop_json_decode(file_get_contents($file));

  return is_array($data) ? $data : FALSE;
}

/**
 * Inspect required modules before running theme setup.
 *
 * @return array
 *   Lists of missing, disabled, and invalid module names, plus configuration status.
 */
function theme_setup_module_requirements($theme) {
  $result = array('missing' => array(), 'disabled' => array(), 'invalid' => array(), 'valid' => TRUE);
  $data = theme_setup_read_json($theme, 'modules/modules.json');
  if (!is_array($data) || !isset($data['modules']) || !is_array($data['modules'])) {
    $result['valid'] = FALSE;
    return $result;
  }

  // Discover installed files, including disabled modules, through Backdrop.
  $available = system_rebuild_module_data();
  foreach (array_unique($data['modules'], SORT_REGULAR) as $module) {
    if (!is_string($module) || !preg_match('/^[a-z][a-z0-9_]*$/', $module)) {
      $result['invalid'][] = is_scalar($module) ? (string) $module : '(invalid value)';
    }
    elseif (!isset($available[$module])) {
      $result['missing'][] = $module;
    }
    elseif (!module_exists($module)) {
      $result['disabled'][] = $module;
    }
  }
  return $result;
}

/**
 * Enable modules required by the theme setup.
 *
 * @param string $theme
 *   Theme machine name.
 *
 * @return bool
 *   TRUE if all required modules are enabled.
 */
function theme_setup_enable_modules($theme) {
  $requirements = theme_setup_module_requirements($theme);
  if (!$requirements['valid'] || $requirements['missing'] || $requirements['invalid']) {
    watchdog('theme_setup', 'Required modules are missing or invalid: @modules', array(
      '@modules' => implode(', ', array_merge($requirements['missing'], $requirements['invalid'])),
    ), WATCHDOG_ERROR);
    return FALSE;
  }

  if (!$requirements['disabled']) {
    return TRUE;
  }

  if (!module_enable($requirements['disabled'], TRUE)) {
    watchdog('theme_setup', 'Failed to enable required modules: @modules', array(
      '@modules' => implode(', ', $requirements['disabled']),
    ), WATCHDOG_ERROR);
    return FALSE;
  }

  foreach ($requirements['disabled'] as $module) {
    if (!module_exists($module)) {
      watchdog('theme_setup', 'Module @module was not enabled.', array('@module' => $module), WATCHDOG_ERROR);
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Import field configuration using Backdrop CMI.
 */
function theme_setup_import_fields($theme) {
  $directory = backdrop_get_path('theme', $theme) . '/settings-content/fields';

  if (!is_dir($directory)) {
    return FALSE;
  }

  $storage = new ConfigFileStorage($directory);

  // Import field definitions before instances.
  foreach (array('field.field.', 'field.instance.') as $prefix) {
    foreach ($storage->listAll($prefix) as $name) {
      $config = config($name);

      // Never overwrite existing configuration.
      if (!$config->isNew()) {
        continue;
      }

      $data = $storage->read($name);

      if (!is_array($data)) {
        return FALSE;
      }

      $config->setData($data);

      // Allow Field API to process the configuration.
      module_invoke_all('config_create', $config);

      $config->save();
    }
  }

  return TRUE;
}

/**
 * Flush selected image style derivatives.
 *
 * @param array|string|null $styles
 *   Image style names, or NULL to flush all styles.
 *
 * @return bool
 *   TRUE if all requested image styles were flushed.
 */
function theme_setup_flush_image_styles($styles = NULL) {
  if (!module_exists('image')) {
    return FALSE;
  }

  $available = image_styles();

  // Flush all styles when none are specified.
  if ($styles === NULL) {
    $styles = array_keys($available);
  }
  elseif (is_string($styles)) {
    $styles = array($styles);
  }
  elseif (!is_array($styles)) {
    return FALSE;
  }

  $success = TRUE;

  foreach (array_unique($styles) as $style_name) {
    if (!is_string($style_name) || !isset($available[$style_name])) {
      $success = FALSE;
      continue;
    }

    // Pass the complete image style definition.
    image_style_flush($available[$style_name]);
  }

  return $success;
}


/**
 * Import a managed file from the theme's sample files.
 *
 * @param string $theme
 *   Theme machine name.
 * @param string $relative_path
 *   Path relative to settings-content/files.
 * @param string $scheme
 *   Destination file scheme: public or private.
 *
 * @return object|false
 *   Managed file entity, or FALSE on failure.
 */
function theme_setup_import_file($theme, $relative_path, $scheme = 'public') {
  if (!is_string($relative_path) ||
      !preg_match('@^[a-zA-Z0-9_./-]+$@', $relative_path) ||
      $scheme !== 'public' && $scheme !== 'private') {
    return FALSE;
  }

  $base = realpath(backdrop_get_path('theme', $theme) . '/settings-content/files');
  $source = realpath(backdrop_get_path('theme', $theme) . '/settings-content/files/' . $relative_path);

  if (!$base || !$source ||
      strpos($source, $base . DIRECTORY_SEPARATOR) !== 0 ||
      !is_file($source) || !is_readable($source)) {
    return FALSE;
  }

  // Prepare the parent directory first.
  $parent = $scheme . '://theme-setup';

  if (!file_prepare_directory(
    $parent,
    FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS
  )) {
    watchdog(
      'theme_setup',
      'Failed to prepare parent directory: @directory',
      array('@directory' => $parent),
      WATCHDOG_ERROR
    );
    return FALSE;
  }

  // Prepare the theme directory.
  $directory = $parent . '/' . $theme;

  if (!file_prepare_directory(
    $directory,
    FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS
  )) {
    watchdog(
      'theme_setup',
      'Failed to prepare theme directory: @directory',
      array('@directory' => $directory),
      WATCHDOG_ERROR
    );
    return FALSE;
  }

  $filename = basename($source);
  $destination = $directory . '/' . $filename;

  // Reuse an already imported managed file.
  $existing = db_select('file_managed', 'f')
    ->fields('f', array('fid'))
    ->condition('uri', $destination)
    ->execute()
    ->fetchField();

  if ($existing) {
    return file_load($existing);
  }

  // Avoid overwriting existing files.
  if (file_exists($destination)) {
    return FALSE;
  }

  $contents = file_get_contents($source);

  if ($contents === FALSE) {
    return FALSE;
  }

  $file = file_save_data($contents, $destination, FILE_EXISTS_ERROR);

  if (!$file) {
    return FALSE;
  }

  $file->status = FILE_STATUS_PERMANENT;
  file_save($file);

  return $file;
}

/**
 * Import taxonomy terms from the theme JSON file.
 */
function theme_setup_import_terms($theme) {
  if (!module_exists('taxonomy')) {
    return FALSE;
  }

  $data = theme_setup_read_json($theme, 'taxonomy/terms.json');

  if (empty($data['terms']) || !is_array($data['terms'])) {
    return FALSE;
  }

  $success = TRUE;
  $vocabularies = array();

  foreach ($data['terms'] as $item) {
    if (!is_array($item) || empty($item['vocabulary']) ||
        empty($item['name']) || !is_string($item['name']) ||
        !is_string($item['vocabulary'])) {
      $success = FALSE;
      continue;
    }

    $vocabulary = $item['vocabulary'];

    // Cache vocabulary existence checks.
    if (!isset($vocabularies[$vocabulary])) {
      $vocabularies[$vocabulary] = (bool) taxonomy_vocabulary_load($vocabulary);
    }

    if (!$vocabularies[$vocabulary]) {
      $success = FALSE;
      continue;
    }

    // Skip existing terms.
    if (taxonomy_term_load_multiple_by_name($item['name'], $vocabulary)) {
      continue;
    }

    // Validate the URL alias before creating the term.
    $alias = '';
    if (isset($item['path'])) {
      if (!is_string($item['path'])) {
        $success = FALSE;
        continue;
      }

      $alias = trim($item['path'], '/');

      if (!$alias || !module_exists('path') ||
          !preg_match('@^[a-zA-Z0-9/_-]+$@', $alias) ||
          db_select('url_alias', 'a')
            ->condition('alias', $alias)
            ->countQuery()
            ->execute()
            ->fetchField()) {
        $success = FALSE;
        continue;
      }
    }

    $parent_tid = 0;

    if (!empty($item['parent'])) {
      $parents = taxonomy_term_load_multiple_by_name(
        $item['parent'],
        $vocabulary
      );

      if (empty($parents)) {
        $success = FALSE;
        continue;
      }

      $parent = reset($parents);
      $parent_tid = $parent->tid;
    }

    // Create the taxonomy term.
    $term = entity_create('taxonomy_term', array(
      'name' => $item['name'],
      'vocabulary' => $vocabulary,
      'description' => isset($item['description']) ? $item['description'] : '',
      'weight' => isset($item['weight']) ? (int) $item['weight'] : 0,
      'parent' => array($parent_tid),
    ));

    taxonomy_term_save($term);

    if (empty($term->tid)) {
      $success = FALSE;
      continue;
    }

    // Save the URL alias.
    if ($alias) {
      $path = array(
        'source' => 'taxonomy/term/' . $term->tid,
        'alias' => $alias,
        'langcode' => LANGUAGE_NONE,
      );

      path_save($path);
    }
  }

  if (!$success) {
    watchdog('theme_setup', 'Some taxonomy terms could not be imported.', array(), WATCHDOG_WARNING);
  }

  return $success;
}

/**
 * Prepare a field value for a node.
 *
 * @return array|false
 *   Field items, or FALSE if the value cannot be prepared.
 */
function theme_setup_prepare_field($theme, $field_name, $value) {
  $field = field_info_field($field_name);

  if (!$field) {
    return FALSE;
  }

  $type = $field['type'];

  // Treat a list of values as a multi-value field.
  $multiple = is_array($value) &&
    array_keys($value) === range(0, count($value) - 1) &&
    !empty($value);

  $values = $multiple ? $value : array($value);

  if (isset($field['cardinality']) &&
      $field['cardinality'] != FIELD_CARDINALITY_UNLIMITED &&
      count($values) > $field['cardinality']) {
    return FALSE;
  }

  $items = array();

  foreach ($values as $item) {
    switch ($type) {
      case 'text':
      case 'text_long':
      case 'text_with_summary':
        $items[] = is_array($item)
          ? $item
          : array('value' => (string) $item);
        break;

      case 'number_integer':
      case 'number_decimal':
      case 'number_float':
        if (!is_numeric($item)) {
          return FALSE;
        }
        $items[] = array('value' => $item);
        break;

      case 'list_boolean':
      case 'list_integer':
      case 'list_float':
      case 'list_text':
        $items[] = array('value' => is_bool($item) ? (int) $item : $item);
        break;

      case 'taxonomy_term_reference':
        if (is_array($item) &&
            !empty($item['vocabulary']) &&
            !empty($item['name'])) {
          $terms = taxonomy_term_load_multiple_by_name(
            $item['name'],
            $item['vocabulary']
          );

          if (!$terms) {
            return FALSE;
          }

          $term = reset($terms);
          $items[] = array('tid' => $term->tid);
        }
        elseif (is_array($item) && !empty($item['tid'])) {
          if (!taxonomy_term_load($item['tid'])) {
            return FALSE;
          }
          $items[] = array('tid' => (int) $item['tid']);
        }
        else {
          return FALSE;
        }
        break;

      case 'entityreference':
          if (!module_exists('entityreference') ||
              !is_array($item) ||
              empty($item['type']) ||
              empty($item['title']) ||
              !is_string($item['type']) ||
              !is_string($item['title'])) {
            return FALSE;
          }

          // Resolve the referenced node using its content type and title.
          $query = new EntityFieldQuery();
          $result = $query
            ->entityCondition('entity_type', 'node')
            ->entityCondition('bundle', $item['type'])
            ->propertyCondition('title', $item['title'])
            ->range(0, 2)
            ->execute();

          $matches = isset($result['node']) ? $result['node'] : array();

          // Reject missing or ambiguous references.
          if (count($matches) !== 1) {
            return FALSE;
          }

          $node = reset($matches);
          $items[] = array('target_id' => (int) $node->entity_id);
          break;

      case 'image':
      case 'file':
        if (!is_string($item)) {
          return FALSE;
        }

        $extension = strtolower(pathinfo($item, PATHINFO_EXTENSION));

        if ($type === 'image' &&
            !in_array($extension, array('jpg', 'jpeg', 'png', 'gif', 'webp'))) {
          return FALSE;
        }

        $file = theme_setup_import_file($theme, $item);

        if (!$file) {
          return FALSE;
        }

        $items[] = array(
          'fid' => $file->fid,
          'display' => 1,
          'description' => '',
        );
        break;

      default:
        // Allow native Field API item structures for other types.
        if (!is_array($item)) {
          return FALSE;
        }

        $items[] = $item;
        break;
    }
  }

  return $items;
}

/**
 * Import sample content from the theme JSON file.
 *
 * @param string $theme
 *   Theme machine name.
 *
 * @return bool
 *   TRUE if all items were imported or already exist.
 */

function theme_setup_import_content($theme) {
  if (!module_exists('node')) {
    return FALSE;
  }

  $data = theme_setup_read_json($theme, 'content/content.json');

  if (empty($data['content']) || !is_array($data['content'])) {
    return FALSE;
  }

  $success = TRUE;
  $imported = 0;
  $types = node_type_get_types();

  // Import referenced sample nodes before nodes that depend on them.
  // Preserve the original order when no dependency exists.
  $pending = $data['content'];
  $ordered = array();
  $scheduled = array();
  $sample_keys = array();

  foreach ($pending as $item) {
    if (is_array($item) && !empty($item['type']) && !empty($item['title'])) {
      $sample_keys[$item['type'] . "\0" . $item['title']] = TRUE;
    }
  }

  while ($pending) {
    $remaining = array();
    $progress = FALSE;

    foreach ($pending as $item) {
      $waiting = FALSE;
      if (is_array($item) && !empty($item['fields']) && is_array($item['fields'])) {
        foreach ($item['fields'] as $field_name => $value) {
          $field = field_info_field($field_name);
          if (!$field || $field['type'] !== 'entityreference') {
            continue;
          }

          $references = is_array($value) && isset($value[0]) ? $value : array($value);
          foreach ($references as $reference) {
            if (is_array($reference) && !empty($reference['type']) && !empty($reference['title'])) {
              $key = $reference['type'] . "\0" . $reference['title'];
              if (isset($sample_keys[$key]) && !isset($scheduled[$key])) {
                $waiting = TRUE;
                break 2;
              }
            }
          }
        }
      }

      if ($waiting) {
        $remaining[] = $item;
        continue;
      }

      $ordered[] = $item;
      if (is_array($item) && !empty($item['type']) && !empty($item['title'])) {
        $scheduled[$item['type'] . "\0" . $item['title']] = TRUE;
      }
      $progress = TRUE;
    }

    if (!$progress) {
      watchdog('theme_setup', 'Circular or unresolved sample content references detected.', array(), WATCHDOG_WARNING);
      $success = FALSE;
      break;
    }
    $pending = $remaining;
  }

  foreach ($ordered as $item) {
    if (!is_array($item) ||
        empty($item['type']) ||
        empty($item['title']) ||
        !isset($types[$item['type']])) {
      $success = FALSE;
      continue;
    }

    $type = $item['type'];
    $alias = '';

    // Validate and check the URL alias.
    if (isset($item['path'])) {
      if (!is_string($item['path'])) {
        $success = FALSE;
        continue;
      }

      $alias = trim($item['path'], '/');

      if ($alias !== '' && (!module_exists('path') ||
          !preg_match('@^[a-zA-Z0-9/_-]+$@', $alias))) {
        $success = FALSE;
        continue;
      }

      // Do not overwrite an existing path alias.
      if ($alias !== '') {
        $existing = db_select('url_alias', 'a')
          ->fields('a', array('source'))
          ->condition('alias', $alias)
          ->execute()
          ->fetchField();

        if ($existing !== FALSE) {
          continue;
        }
      }
    }

    // Create a new content entity.
    $node = new Node();
    $node->type = $type;
    node_object_prepare($node);

    $node->title = $item['title'];
    $node->uid = 1;
    $node->status = isset($item['status']) ? (int) $item['status'] : 1;
    $node->promote = !empty($item['promote']) ? 1 : 0;
    $node->sticky = !empty($item['sticky']) ? 1 : 0;
    $node->language = LANGUAGE_NONE;

    // Prepare the body field.
    if (isset($item['body'])) {
      $body = is_array($item['body'])
        ? $item['body']
        : array('value' => $item['body']);

      $node->body[LANGUAGE_NONE][0] = $body;
    }

    // Prepare custom fields.
    $valid = TRUE;

    if (!empty($item['fields'])) {
      foreach ($item['fields'] as $field_name => $value) {
        $instance = field_info_instance('node', $field_name, $type);

        if (!$instance) {
          $valid = FALSE;

          watchdog(
            'theme_setup',
            'Field @field is not attached to content type @type.',
            array(
              '@field' => $field_name,
              '@type' => $type,
            ),
            WATCHDOG_WARNING
          );

          break;
        }

        $values = theme_setup_prepare_field(
          $theme,
          $field_name,
          $value
        );

        if ($values === FALSE) {
          $valid = FALSE;

          watchdog(
            'theme_setup',
            'Could not prepare field @field for @title.',
            array(
              '@field' => $field_name,
              '@title' => $item['title'],
            ),
            WATCHDOG_WARNING
          );

          break;
        }

        $node->{$field_name}[LANGUAGE_NONE] = $values;
      }
    }

    if (!$valid) {
      $success = FALSE;
      continue;
    }

    // Save the content.
    node_save($node);

    if (empty($node->nid)) {
      watchdog(
        'theme_setup',
        'Failed to save content: @title',
        array('@title' => $item['title']),
        WATCHDOG_ERROR
      );

      $success = FALSE;
      continue;
    }

    $imported++;

    // Save the custom URL alias.
    if ($alias) {
      $path = array(
        'source' => 'node/' . $node->nid,
        'alias' => $alias,
        'langcode' => LANGUAGE_NONE,
      );

      path_save($path);
    }
  }

  // Log partial import failures.
  if (!$success) {
    watchdog(
      'theme_setup',
      'Some sample content could not be imported.',
      array(),
      WATCHDOG_WARNING
    );
  }

  return $success;
}

/**
 * Import menu links from the theme JSON file.
 *
 * @param string $theme
 *   Theme machine name.
 *
 * @return bool
 *   TRUE if all links were imported or already exist.
 */
function theme_setup_import_menu_links($theme) {
  if (!module_exists('menu')) {
    return FALSE;
  }

  $data = theme_setup_read_json($theme, 'menu/links.json');

  if (empty($data['links']) || !is_array($data['links'])) {
    return FALSE;
  }

  $success = TRUE;
  $pending = $data['links'];
  $menu_names = array_keys(menu_get_menus());

  // Retry links whose parents have not yet been imported.
  while (!empty($pending)) {
    $remaining = array();
    $progress = FALSE;

    foreach ($pending as $item) {
      if (!is_array($item) ||
          empty($item['menu']) ||
          empty($item['title']) ||
          !isset($item['path']) ||
          !is_string($item['path']) ||
          !in_array($item['menu'], $menu_names)) {
        $success = FALSE;
        continue;
      }

      $menu = $item['menu'];
      $title = $item['title'];
      $path = trim($item['path']);

      if ($path === '') {
        $success = FALSE;
        continue;
      }

      if (!url_is_external($path) && $path !== '<front>') {
        $path = backdrop_get_normal_path(trim($path, '/'));
      }

      // Find an existing link by menu, path, and title.
      $existing = db_select('menu_links', 'ml')
        ->fields('ml', array('mlid'))
        ->condition('menu_name', $menu)
        ->condition('link_path', $path)
        ->condition('link_title', $title)
        ->execute()
        ->fetchField();

      if ($existing) {
        $progress = TRUE;
        continue;
      }

      $parent_id = 0;

      // Resolve parent menu link by title within the same menu.
      if (!empty($item['parent'])) {
        $parent_title = trim($item['parent']);

        $parent_id = db_select('menu_links', 'ml')
          ->fields('ml', array('mlid'))
          ->condition('menu_name', $menu)
          ->condition('link_title', $parent_title)
          ->execute()
          ->fetchField();

        if (!$parent_id) {
          $remaining[] = $item;
          continue;
        }
      }

      $link = array(
        'menu_name' => $menu,
        'link_title' => $title,
        'link_path' => $path,
        'plid' => (int) $parent_id,
        'weight' => isset($item['weight']) ? (int) $item['weight'] : 0,
        'expanded' => !empty($item['expanded']) ? 1 : 0,
        'hidden' => !empty($item['hidden']) ? 1 : 0,
        'langcode' => LANGUAGE_NONE,
        'module' => 'menu',
      );

      if (!menu_link_save($link)) {
        $success = FALSE;
      }
      else {
        $progress = TRUE;
      }
    }

    if (!$progress) {
      $success = FALSE;
      watchdog(
        'theme_setup',
        'Could not resolve some menu link parents.',
        array(),
        WATCHDOG_WARNING
      );
      break;
    }

    $pending = $remaining;
  }

  menu_cache_clear_all();

  return $success;
}

/**
 * Update Views taxonomy filters using actual term IDs.
 *
 * @param string $theme
 *   The theme machine name.
 *
 * @return bool
 *   TRUE if all filters were processed successfully.
 */
function theme_setup_update_views_filters($theme) {
  if (!module_exists('views') || !module_exists('taxonomy')) {
    return FALSE;
  }

  $data = theme_setup_read_json($theme, 'views/filters.json');

  if (empty($data['filters']) || !is_array($data['filters'])) {
    return FALSE;
  }

  $success = TRUE;
  $updated = FALSE;
  $views = array();

  foreach ($data['filters'] as $item) {
    // Validate the filter configuration.
    if (!is_array($item) ||
        empty($item['view']) ||
        empty($item['display']) ||
        empty($item['filter']) ||
        empty($item['vocabulary']) ||
        empty($item['term'])) {
      $success = FALSE;
      continue;
    }

    $view_name = $item['view'];
    $display = $item['display'];
    $filter_name = $item['filter'];
    $vocabulary = $item['vocabulary'];
    $term_name = $item['term'];

    // Find the term within its vocabulary.
    $terms = taxonomy_term_load_multiple_by_name(
      $term_name,
      $vocabulary
    );

    if (count($terms) !== 1) {
      watchdog(
        'theme_setup',
        'Expected exactly one term @term in vocabulary @vocabulary.',
        array(
          '@term' => $term_name,
          '@vocabulary' => $vocabulary,
        ),
        WATCHDOG_WARNING
      );

      $success = FALSE;
      continue;
    }

    $term = reset($terms);
    $tid = (int) $term->tid;

    // Load each View only once.
    if (!isset($views[$view_name])) {
      $config = config('views.view.' . $view_name);

      if ($config->isNew()) {
        watchdog(
          'theme_setup',
          'View @view does not exist.',
          array('@view' => $view_name),
          WATCHDOG_WARNING
        );

        $success = FALSE;
        continue;
      }

      $views[$view_name] = array(
        'config' => $config,
        'data' => $config->get(),
        'changed' => FALSE,
      );
    }

    $view_data = &$views[$view_name]['data'];

    // Check that the display exists.
    if (!isset($view_data['display'][$display])) {
      watchdog(
        'theme_setup',
        'Display @display not found in View @view.',
        array(
          '@display' => $display,
          '@view' => $view_name,
        ),
        WATCHDOG_WARNING
      );

      $success = FALSE;
      unset($view_data);
      continue;
    }

    // Check that the filter exists in this display.
    if (!isset(
      $view_data['display'][$display]['display_options']
        ['filters'][$filter_name]
    )) {
      watchdog(
        'theme_setup',
        'Filter @filter not found in View @view, display @display.',
        array(
          '@filter' => $filter_name,
          '@view' => $view_name,
          '@display' => $display,
        ),
        WATCHDOG_WARNING
      );

      $success = FALSE;
      unset($view_data);
      continue;
    }

    // Get the filter configuration.
    $filter = &$view_data['display'][$display]
      ['display_options']['filters'][$filter_name];

    // Replace the old term ID with the actual term ID.
    $new_value = array(
      $tid => (string) $tid,
    );

    // Update only when the value has changed.
    if (!isset($filter['value']) ||
        $filter['value'] !== $new_value) {
      $filter['value'] = $new_value;
      $views[$view_name]['changed'] = TRUE;
    }

    unset($filter, $view_data);
  }

  // Save each modified View once.
  foreach ($views as $view_name => $view) {
    if (!$view['changed']) {
      continue;
    }

    $config = $view['config'];
    $config->setData($view['data']);

    try {
      $config->save();
      $updated = TRUE;
    }
    catch (Exception $e) {
      watchdog(
        'theme_setup',
        'Failed to save View @view: @error',
        array(
          '@view' => $view_name,
          '@error' => $e->getMessage(),
        ),
        WATCHDOG_ERROR
      );

      $success = FALSE;
    }
  }

  // Clear Views caches once after saving.
  if ($updated) {
    views_invalidate_cache();
  }

  return $success;
}

/**
 * Apply layout settings from the theme JSON configuration.
 *
 * @param string $theme
 *   The machine name of the theme.
 *
 * @return bool
 *   TRUE if all layouts were processed successfully.
 */
function theme_setup_apply_layout_settings($theme) {
  $data = theme_setup_read_json($theme, 'layouts/settings.json');

  if (!is_array($data) || empty($data['layouts']) || !is_array($data['layouts'])) {
    return FALSE;
  }

  $success = TRUE;

  foreach ($data['layouts'] as $item) {
    if (empty($item['name'])) {
      $success = FALSE;
      continue;
    }

    // Load the existing layout.
    $layout = layout_load($item['name']);

    if (!$layout) {
      watchdog('theme_setup', 'Layout not found: @name', array(
        '@name' => $item['name'],
      ), WATCHDOG_WARNING);
      $success = FALSE;
      continue;
    }

    $changed = FALSE;

    // Change the layout template when specified.
    if (isset($item['template'])) {
      if ($layout->layout_template !== $item['template']) {
        $layout->layout_template = $item['template'];
        $changed = TRUE;
      }
    }

    // Change the page title display setting.
    if (isset($item['title_display'])) {
      if (!isset($layout->settings['title_display']) ||
          $layout->settings['title_display'] !== $item['title_display']) {
        $layout->settings['title_display'] = $item['title_display'];
        $changed = TRUE;
      }
    }

    // Change the page title when specified.
    if (array_key_exists('title', $item)) {
      if (!isset($layout->settings['title']) ||
          $layout->settings['title'] !== $item['title']) {
        $layout->settings['title'] = $item['title'];
        $changed = TRUE;
      }
    }

    // Save only layouts that have changed.
    if ($changed) {
      $layout->save();
    }
  }

  return $success;
}

/**
 * Configure layout blocks using the theme JSON configuration.
 *
 * Supports adding, removing, moving, and configuring blocks.
 *
 * @param string $theme
 *   The machine name of the theme.
 *
 * @return bool
 *   TRUE if all layout blocks were processed successfully.
 */
function theme_setup_configure_layout_blocks($theme) {
  $data = theme_setup_read_json($theme, 'layouts/blocks.json');

  if (empty($data['layouts']) || !is_array($data['layouts'])) {
    return FALSE;
  }

  $success = TRUE;
  $layouts_changed = FALSE;

  foreach ($data['layouts'] as $item) {
    if (empty($item['name']) || empty($item['blocks']) || !is_array($item['blocks'])) {
      $success = FALSE;
      continue;
    }

    // Load the existing layout.
    $layout = layout_load($item['name']);

    if (!$layout) {
      watchdog('theme_setup', 'Layout not found: @name', array(
        '@name' => $item['name'],
      ), WATCHDOG_WARNING);
      $success = FALSE;
      continue;
    }

    $changed = FALSE;

    // Process removals before additions.
    foreach (array('remove', 'add') as $action) {
      foreach ($item['blocks'] as $definition) {
        if (!isset($definition['action']) || $definition['action'] !== $action) {
          continue;
        }

        if (empty($definition['plugin'])) {
          $success = FALSE;
          continue;
        }

        $plugin = $definition['plugin'];

        // Remove all matching block instances.
        if ($action === 'remove') {
          foreach ($layout->content as $uuid => $block) {
            if ($block->plugin === $plugin) {
              $layout->removeBlock($uuid);
              $changed = TRUE;
            }
          }
          continue;
        }

        // Validate the target region.
        if (empty($definition['region']) ||
            !isset($layout->positions[$definition['region']])) {
          watchdog('theme_setup', 'Invalid region for block @plugin in layout @layout.', array(
            '@plugin' => $plugin,
            '@layout' => $item['name'],
          ), WATCHDOG_WARNING);
          $success = FALSE;
          continue;
        }

        $region = $definition['region'];
        $position = NULL;

        if (array_key_exists('position', $definition)) {
          if (!is_int($definition['position']) || $definition['position'] < 0) {
            $success = FALSE;
            continue;
          }
          $position = $definition['position'];
        }

        // Find an existing instance of this block.
        $block = NULL;

        foreach ($layout->content as $existing_block) {
          if ($existing_block->plugin === $plugin) {
            $block = $existing_block;
            break;
          }
        }

        if ($block) {
          // Get the current block position.
          $current_region = $layout->getBlockPosition($block->uuid);
          $current_position = $layout->getBlockPosition($block->uuid, 'position');

          // Keep the current position when none is specified.
          if ($position === NULL && $current_region === $region) {
            // No position change required.
          }
          else {
            $region_count = count($layout->positions[$region]);
            $target_position = $position === NULL
              ? NULL
              : min($position, max(0, $region_count - 1));

            if ($current_region !== $region ||
                ($target_position !== NULL && $current_position !== $target_position)) {
              $layout->setBlockPosition($block->uuid, $region, $target_position);
              $changed = TRUE;
            }
          }
        }
        else {
          // Split the plugin into module and delta.
          $parts = explode(':', $plugin, 2);

          if (count($parts) !== 2 || empty($parts[0]) || empty($parts[1])) {
            $success = FALSE;
            continue;
          }

          list($module, $delta) = $parts;

          // Check that the requested block is available.
          $available = layout_get_block_info($module, $delta);

          if (!$available) {
            watchdog('theme_setup', 'Block plugin not found: @plugin', array(
              '@plugin' => $plugin,
            ), WATCHDOG_WARNING);
            $success = FALSE;
            continue;
          }

          // Add the block at the end of the region first.
          $block = $layout->addBlock($module, $delta, $region);

          if (!$block) {
            $success = FALSE;
            continue;
          }

          // Apply the requested position within the region boundaries.
          if ($position !== NULL) {
            $last_position = count($layout->positions[$region]) - 1;
            $target_position = min($position, $last_position);

            if ($target_position < $last_position) {
              $layout->setBlockPosition($block->uuid, $region, $target_position);
            }
          }

          $changed = TRUE;
        }

        // Apply optional block settings.
        if (!empty($definition['settings']) && is_array($definition['settings'])) {
          foreach ($definition['settings'] as $key => $value) {
            if (!array_key_exists($key, $block->settings) ||
                $block->settings[$key] !== $value) {
              $block->settings[$key] = $value;
              $changed = TRUE;
            }
          }
        }
      }
    }

    // Save only layouts that have changed.
    if ($changed) {
      $layout->save();
      $layouts_changed = TRUE;
    }
  }

  // Rebuild layout caches after configuration changes.
  if ($layouts_changed) {
    layout_reset_caches();
  }

  return $success;
}
