# Backdrop Theme Setup — Example Theme

A working example of **Backdrop Theme Setup**, a JSON-driven library for preparing sample content and site configuration from a Backdrop CMS theme. The library uses Backdrop APIs and Configuration Management (CMI) rather than requiring theme designers to write custom import routines.

The example has been tested locally on Backdrop CMS. Test on a fresh installation or with a database backup before using it on an existing site: imports can create content and change site configuration.

## Installation

1. Copy the example theme into your Backdrop installation's `themes/` directory. Keep the theme's machine name and directory name consistent with its `.info`, `theme-settings.php`, and submit handler. For the current example, use `backdrop_theme_setup` and visit `admin/appearance/settings/backdrop_theme_setup`.
2. Enable the theme under **Appearance** and open its settings page.
3. Review the JSON examples under `settings-content/` and check that the required content types, vocabularies, menus, layouts, and any contributed modules are available.
4. If the example declares module dependencies, use **Enable required modules** before importing fields. When field or content import requires disabled dependencies, the setup handler can enable available dependencies first.
5. Select **Import field configurations** if the example includes field CMI configuration, then import taxonomy terms, sample content, and menu links as needed. Fields must exist before content that uses them is imported.
6. Apply layout settings, configure layout blocks, or update Views filters only after checking the prerequisites described below.

**Dependency safety:** If a required module is missing or its name is invalid, the related setup options are disabled and a warning is shown. Install missing modules through Backdrop's Project Installer, then return to the theme settings. Already enabled modules are not enabled again.

## Sample content and images

The `settings-content/content/content.json` example includes:

- **Theme Setup Demo** (`page`) — `demo/theme-setup`
- **Theme Setup Documentation** (`page`) — `demo/theme-setup-docs`
- **Introduction to Backdrop CMS** (`post`) — `articles/introduction-to-backdrop`
- **Building Websites with Tailwind CSS** (`post`) — `articles/tailwind-css`

The two example posts reference an image using the `field_image` field:

```json
"fields": {
  "field_image": "theme-setup-demo.png"
}
```

Place `theme-setup-demo.png` in `settings-content/files/` and ensure the `post` content type has a compatible `field_image` field before importing. You may instead use separate image files, provided their filenames match the JSON values exactly.

The sample image is imported as a managed file by the library when the field and source file are available. Existing content aliases may cause items to be skipped; use a clean test installation when verifying a full import.

**Known limitation:** The sample posts intentionally omit `field_tags`. Importing a taxonomy term reference such as `"field_tags": "Articles"` may fail if the referenced term does not already exist. An autocomplete widget in the content editing form does not itself guarantee automatic term creation during a programmatic import. For now, omit this field from the sample JSON or create/import the term first. Improved handling is planned for a future update.

## Configuration files

| File or directory | Purpose | Notes |
| --- | --- | --- |
| `modules/modules.json` | Declare required modules | Missing modules must be installed before dependent imports |
| `fields/` | Import field definitions and instances through CMI | Required modules must be enabled first |
| `taxonomy/terms.json` | Import taxonomy terms | Required vocabularies must exist |
| `content/content.json` | Import sample nodes and field values | Content types and fields must exist; image filenames must match |
| `files/` | Source images and other sample files | Referenced by filename in sample content |
| `menu/links.json` | Import menu links | Target menus and referenced links/content must exist |
| `views/filters.json` | Update configured Views filters | Requires matching existing Views, displays, and filter identifiers |
| `layouts/settings.json` | Apply layout settings | Verify layout names and templates first |
| `layouts/blocks.json` | Configure layout blocks | Verify layout regions and block plugin identifiers first |

All paths in the table are relative to `settings-content/`. Only select an operation if its corresponding example files and prerequisites are present.

## Optional integration examples

**Views filters:** The example `views/filters.json` targets an existing View named `home_project`, a `block` display, and filter `field_portfolio_main_tid`. It is not a generic configuration for a stock installation. Adjust it to match your own View before using **Update Views filters**.

**Layouts:** Check that the `home` layout, `boxton` layout template, the `header` region, and `system:main-menu` block are available before applying the corresponding layout examples. Layout operations can change the site's existing block placement and configuration.

**Image styles:** Existing image styles can be selected in the settings form for flushing; no separate JSON file is required for that operation.

## Known limitations and precautions

- This repository demonstrates a library integrated into a **theme**, not a standalone Backdrop module. Its setup actions are optional and should be reviewed before use.
- Imports are not transactional and do not provide a general undo/rollback operation. Back up an existing site before running them.
- Disabling the theme does not automatically remove content or layout changes made by setup actions.
- A requested path alias already in use can cause content to be skipped.
- Layout block matching by plugin identifier may not distinguish multiple instances of the same plugin.
- Taxonomy term reference import, including `field_tags`, needs additional handling for terms that do not yet exist.
- Custom Views and layout examples are installation-specific and should not be applied without checking their dependencies.

## Credits

- Created for [Backdrop CMS](https://backdropcms.org/) by [shabib-m](https://github.com/shabib-m).
- Developed with AI assistance.

## License

Backdrop Theme Setup is free and open-source software licensed under the GNU General Public License, version 2 or later (GPL-2.0-or-later).

See [LICENSE.txt](LICENSE.txt) for the full license text.
