# Backdrop Theme Setup — runnable example theme

A review/demo package for the JSON-driven Theme Setup library. **Not production-ready or integration-tested on a running Backdrop instance.** PHP syntax and JSON validity are checked; behavior must be tested locally.

## Installation

1. Place this directory at `themes/theme_setup` (the machine name **must** be `theme_setup` for the provided submit handler).
2. Enable the theme at **Appearance**, then visit `admin/appearance/settings/theme_setup`.
3. Use a disposable Backdrop Standard installation or a database backup. These actions create content and change site configuration.
4. Confirm the **tags** vocabulary, **page** content type, **main-menu** menu, **home** layout, and **boxton** layout template exist. Names may differ by installation profile.
5. Select **Import taxonomy terms**, **Import sample content**, and **Import menu links**; save. You should get two pages at `/demo/theme-setup` and `/demo/theme-setup-docs`, a tag term, and two menu links.
6. Test **Apply layout settings** only if you want to modify the existing `home` layout. It changes the home layout title/template settings.
7. Test **Configure layout blocks** only after confirming `system:main-menu` is available and `header` is a region of `home`. The example adds/moves this block.
8. **Views filters:** the `views/filters.json` file is an **integration example only**. It requires an existing View named `home_project` with a `block` display and filter ID `field_portfolio_main_tid` configured against the `tags` vocabulary. Do not check this option on a stock installation. Edit the JSON to match your own View.
9. **Image styles:** select any existing image styles in the settings form; no JSON is required for this operation.

## Sample files (`settings-content/files/`)

- `theme-setup-demo.png` is a sample PNG bundled to demonstrate the managed-file import helper.
- **The default content example does not automatically import this file**, because a stock Backdrop `page` content type may not have an image field. To test image import, first add an image field named `field_image` to the `page` content type, then add the following property to one item in `content/content.json`:

  ```json
  "fields": {"field_image": "theme-setup-demo.png"}
  ```

- Run **Import sample content** on a clean test installation (or use a new unused page alias); existing page aliases are skipped. The file should be copied into managed public files under `theme-setup/theme_setup/`.

## JSON files

| File | Function | Requirements |
| --- | --- | --- |
| `taxonomy/terms.json` | `theme_setup_import_terms()` | `tags` vocabulary and Path module for alias |
| `content/content.json` | `theme_setup_import_content()` | `page` content type and Path module |
| `menu/links.json` | `theme_setup_import_menu_links()` | `main-menu` and Path aliases from content |
| `views/filters.json` | `theme_setup_update_views_filters()` | **Custom View prerequisite** (see above) |
| `layouts/settings.json` | `theme_setup_apply_layout_settings()` | `home` layout, `boxton` template |
| `layouts/blocks.json` | `theme_setup_configure_layout_blocks()` | `home` layout, `header` region, `system:main-menu` block |

## Important limitations for reviewers

- This is a demonstration **theme**, not a standalone Backdrop module. A theme is normally responsible for presentation; this experiment deliberately explores optional site setup actions through theme settings. Feedback on architectural boundaries is welcome.
- The content importer skips a requested path if the alias is already in use; it does not prove ownership of the existing content.
- The layout block importer matches existing blocks by plugin identifier; multiple instances of the same plugin need further design work.
- Operations are not transactional and do not provide an undo/rollback action.
- Existing content and layouts are not removed by disabling the theme.
- The submit handler removes one-time checkbox values from `$form_state['values']`, but the ordering relative to Backdrop's standard settings save handler should be verified in a real installation.
- The image-style flush uses `image_style_flush($style_definition)` rather than passing a string name.

## Suggested review questions

1. Should this remain an opt-in theme helper, or be implemented as a companion module?
2. What is the safest way to record imported objects and make reruns idempotent?
3. How should dependencies, partial failures, and rollback be handled?
4. Would a declarative JSON schema and automated tests make this reusable for other themes?
