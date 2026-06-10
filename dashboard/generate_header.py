import os

# Define paths
DASHBOARD_DIR = 'dashboard'
FIRMWARE_HEADER = 'firmware/src/server/index_html.h'

# Read components
with open(os.path.join(DASHBOARD_DIR, 'index.html'), 'r') as f:
    html = f.read()

with open(os.path.join(DASHBOARD_DIR, 'css', 'style.css'), 'r') as f:
    css = f.read()

with open(os.path.join(DASHBOARD_DIR, 'js', 'script.js'), 'r') as f:
    js = f.read()

# Combine
# We replace the link and script tags with the actual content
full_html = html.replace('<link rel="stylesheet" href="css/style.css">', f'<style>\n{css}\n</style>')
full_html = full_html.replace('<script src="js/script.js"></script>', f'<script>\n{js}\n</script>')

# Write to header
header_content = f"""#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
{full_html}
)rawliteral";
"""

with open(FIRMWARE_HEADER, 'w') as f:
    f.write(header_content)

print(f"Successfully generated {FIRMWARE_HEADER}")
