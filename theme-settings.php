<?php

/**
 * Implements hook_form_FORM_ID_alter().
 */
function backdrop_theme_setup_form_system_theme_settings_alter(&$form, &$form_state) {
  $theme = 'backdrop_theme_setup';
  $library = backdrop_get_path('theme', $theme) . '/includes/theme-setup.inc';
  if (is_file($library)) {
    require_once $library;
  }
  $requirements = function_exists('theme_setup_module_requirements')
    ? theme_setup_module_requirements($theme)
    : array('valid' => FALSE, 'missing' => array(), 'disabled' => array(), 'invalid' => array());
  $blocked = !$requirements['valid'] || $requirements['missing'] || $requirements['invalid'];
  $needs_enable = !empty($requirements['disabled']);

  // Theme setup vertical tabs.
  $form['theme_setup_tabs'] = array(
    '#type' => 'vertical_tabs',
    '#title' => t('Theme setup'),
    '#weight' => -10,
  );

  // Required modules and field configurations.
  $form['theme_setup_dependencies'] = array(
    '#type' => 'fieldset',
    '#title' => t('Modules and fields'),
    '#group' => 'theme_setup_tabs',
  );
  $form['theme_setup_dependencies']['enable_modules'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable required modules'),
    '#description' => t('Enable modules defined in modules/modules.json.'),
    '#default_value' => 0,
  );
  $form['theme_setup_dependencies']['import_fields'] = array(
    '#type' => 'checkbox',
    '#title' => t('Import field configurations'),
    '#description' => t('Import field definitions and instances using Backdrop CMI.'),
    '#default_value' => 0,
  );

  // Taxonomy settings.
  $form['theme_setup_taxonomy'] = array(
    '#type' => 'fieldset',
    '#title' => t('Taxonomy settings'),
    '#group' => 'theme_setup_tabs',
  );

  $form['theme_setup_taxonomy']['import_terms'] = array(
    '#type' => 'checkbox',
    '#title' => t('Import taxonomy terms'),
    '#default_value' => 0,
  );

  // Content settings.
  $form['theme_setup_content'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content settings'),
    '#group' => 'theme_setup_tabs',
  );

  $form['theme_setup_content']['import_content'] = array(
    '#type' => 'checkbox',
    '#title' => t('Import sample content'),
    '#default_value' => 0,
  );

  // Menu settings.
  $form['theme_setup_menu'] = array(
    '#type' => 'fieldset',
    '#title' => t('Menu settings'),
    '#group' => 'theme_setup_tabs',
  );

  $form['theme_setup_menu']['import_menu_links'] = array(
    '#type' => 'checkbox',
    '#title' => t('Import menu links'),
    '#default_value' => 0,
  );

  // Views settings.
  $form['theme_setup_views'] = array(
    '#type' => 'fieldset',
    '#title' => t('Views settings'),
    '#group' => 'theme_setup_tabs',
  );

  $form['theme_setup_views']['update_views_filters'] = array(
    '#type' => 'checkbox',
    '#title' => t('Update Views taxonomy filters'),
    '#description' => t('Update taxonomy term IDs in Views filters using the settings-content/views/filters.json file. Import taxonomy terms first.'),
    '#default_value' => 0,
  );

  // Layout settings.
  $form['theme_setup_layout'] = array(
    '#type' => 'fieldset',
    '#title' => t('Layout settings'),
    '#group' => 'theme_setup_tabs',
  );
  $form['theme_setup_layout']['layout_settings'] = array(
    '#type' => 'checkbox',
    '#title' => t('Apply layout settings'),
    '#description' => t('Configure existing layouts using settings.json.'),
    '#default_value' => 0,
  );
  $form['theme_setup_layout']['layout_blocks'] = array(
    '#type' => 'checkbox',
    '#title' => t('Configure layout blocks'),
    '#description' => t('Add, remove, and configure blocks using blocks.json.'),
    '#default_value' => 0,
  );

  // Image style settings.
  $form['theme_setup_flush_image'] = array(
    '#type' => 'fieldset',
    '#title' => t('Image styles'),
    '#group' => 'theme_setup_tabs',
  );
  $form['theme_setup_flush_image']['flush_image_styles'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Flush image styles'),
    '#description' => t('Select the image styles to flush when saving the theme settings.'),
    '#options' => array(),
    '#default_value' => array(),
  );

  if (module_exists('image')) {
    foreach (image_styles() as $style_name => $style) {
      $form['theme_setup_flush_image']['flush_image_styles']['#options'][$style_name] =
        !empty($style['label']) ? $style['label'] : $style_name;
    }
  }
  else {
    $form['theme_setup_flush_image']['flush_image_styles']['#disabled'] = TRUE;
  }

  // Custom submit handler.
  $form['#submit'][] = 'backdrop_theme_setup_theme_setup_submit';
  
	// Shared dependency status and message.
	$blocked_message = '';

	if (!empty($requirements['invalid'])) {
	  $blocked_message = t(
		'Invalid module names: @modules.',
		array('@modules' => implode(', ', $requirements['invalid']))
	  );
	}
	elseif (!empty($requirements['missing'])) {
	  $blocked_message = t(
		'Missing required modules: @modules. Install them using the Project Installer.',
		array('@modules' => implode(', ', $requirements['missing']))
	  );
	}
	elseif ($blocked) {
	  $blocked_message = t(
		'Module requirements could not be verified.'
	  );
	}

	// Apply the same dependency status to all three options.
	$dependency_options = array(
	  'theme_setup_dependencies' => array(
		'enable_modules',
		'import_fields',
	  ),
	  'theme_setup_content' => array(
		'import_content',
	  ),
	);

	foreach ($dependency_options as $group => $options) {
	  foreach ($options as $option) {
		if (isset($form[$group][$option]) && $blocked) {
		  $form[$group][$option]['#disabled'] = TRUE;

		  // Preserve the original description.
		  $description = isset($form[$group][$option]['#description'])
			? $form[$group][$option]['#description']
			: '';

		  $form[$group][$option]['#description'] =
			$description . '<div class="messages warning">' .
			check_plain($blocked_message) .
			'</div>';
		}
	  }
	}
}

/**
 * Submit handler for theme setup settings.
 */
function backdrop_theme_setup_theme_setup_submit($form, &$form_state) {
  $enable_modules = !empty($form_state['values']['enable_modules']);
  $import_fields = !empty($form_state['values']['import_fields']);
  $import_terms = !empty($form_state['values']['import_terms']);
  $import_content = !empty($form_state['values']['import_content']);
  $import_menu_links = !empty($form_state['values']['import_menu_links']);
  $update_views_filters = !empty($form_state['values']['update_views_filters']);
  $apply_layout_settings = !empty($form_state['values']['layout_settings']);
  $configure_layout_blocks = !empty($form_state['values']['layout_blocks']);

  // Get the selected image styles.
  $styles = array();

  if (!empty($form_state['values']['flush_image_styles']) &&
      is_array($form_state['values']['flush_image_styles'])) {
    $styles = array_values(array_filter(
      $form_state['values']['flush_image_styles']
    ));
  }

  // Do not persist setup actions in theme settings.
  unset(
    $form_state['values']['enable_modules'],
    $form_state['values']['import_fields'],
    $form_state['values']['import_terms'],
    $form_state['values']['import_content'],
    $form_state['values']['import_menu_links'],
    $form_state['values']['update_views_filters'],
    $form_state['values']['layout_settings'],
    $form_state['values']['layout_blocks'],
    $form_state['values']['flush_image_styles']
  );

  // Stop if no setup option was selected.
  if (!$enable_modules &&
      !$import_fields &&
      !$import_terms &&
      !$import_content &&
      !$import_menu_links &&
      !$update_views_filters &&
      !$apply_layout_settings &&
      !$configure_layout_blocks &&
      empty($styles)) {
    return;
  }

  $theme = 'backdrop_theme_setup';
  $file = backdrop_get_path('theme', $theme) . '/includes/theme-setup.inc';

  if (!is_file($file)) {
    backdrop_set_message(t('Theme setup library not found.'), 'error');
    return;
  }

  require_once $file;

  // Recheck on submit; never rely solely on disabled form controls.
  $requirements = theme_setup_module_requirements($theme);
  if (($enable_modules || $import_fields || $import_content) &&
      (!$requirements['valid'] || $requirements['missing'] || $requirements['invalid'])) {
    backdrop_set_message(t('Cannot proceed. Missing or invalid required modules: @modules', array(
      '@modules' => implode(', ', array_merge($requirements['missing'], $requirements['invalid'])),
    )), 'error');
    return;
  }

	// Enable required modules only when needed.
	if ($enable_modules || $import_fields || $import_content) {
	  if (!empty($requirements['disabled'])) {
		if (!theme_setup_enable_modules($theme)) {
		  backdrop_set_message(t('Failed to enable required modules.'), 'error');
		  return;
		}

		backdrop_set_message(t('Required modules enabled successfully.'));
	  }
	  elseif ($enable_modules) {
		backdrop_set_message(t('All required modules are already enabled.'));
	  }
	}

  if ($import_fields) {
    if (theme_setup_import_fields($theme)) {
      backdrop_set_message(t('Field configurations imported successfully.'));
    }
    else {
      backdrop_set_message(t('Failed to import field configurations.'), 'error');
      return;
    }
  }

  // Import taxonomy terms.
  if ($import_terms) {
    if (theme_setup_import_terms($theme)) {
      backdrop_set_message(t('Taxonomy terms imported successfully.'));
    }
    else {
      backdrop_set_message(t('Some taxonomy terms could not be imported.'), 'warning');
    }
  }

  // Import sample content.
  if ($import_content) {
    if (theme_setup_import_content($theme)) {
      backdrop_set_message(t('Sample content imported successfully.'));
    }
    else {
      backdrop_set_message(t('Some sample content could not be imported.'), 'warning');
    }
  }

  // Import menu links.
  if ($import_menu_links) {
    if (theme_setup_import_menu_links($theme)) {
      backdrop_set_message(t('Menu links imported successfully.'));
    }
    else {
      backdrop_set_message(t('Some menu links could not be imported.'), 'warning');
    }
  }

  // Update taxonomy term IDs in Views filters.
  if ($update_views_filters) {
    if (theme_setup_update_views_filters($theme)) {
      backdrop_set_message(t('Views taxonomy filters updated successfully.'));
    }
    else {
      backdrop_set_message(t('Some Views taxonomy filters could not be updated.'), 'warning');
    }
  }

  // Apply layout settings.
  if ($apply_layout_settings) {
    if (theme_setup_apply_layout_settings($theme)) {
      backdrop_set_message(t('Layout settings applied successfully.'));
    }
    else {
      backdrop_set_message(t('Some layout settings could not be applied.'), 'warning');
    }
  }

  // Configure layout blocks.
  if ($configure_layout_blocks) {
    if (theme_setup_configure_layout_blocks($theme)) {
      backdrop_set_message(t('Layout blocks configured successfully.'));
    }
    else {
      backdrop_set_message(t('Some layout blocks could not be configured.'), 'warning');
    }
  }

  // Flush only the selected image styles.
  if (!empty($styles)) {
    if (theme_setup_flush_image_styles($styles)) {
      backdrop_set_message(t('Selected image styles have been flushed.'));
    }
    else {
      backdrop_set_message(t('Some image styles could not be flushed.'), 'warning');
    }
  }
}
