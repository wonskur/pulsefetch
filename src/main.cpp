#!/usr/bin/env python3
import re
import os
import sys

def parse_inc_file(path):
    logos = []
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find each logo definition
    pattern = r'#ifdef FASTFETCH_DATATEXT_LOGO_(\w+)\s*//\s*(.+?)\s*\{\s*\.names\s*=\s*\{([^}]+)\},?\s*(?:\.type\s*=\s*[^,]+,?\s*)?\.lines\s*=\s*FASTFETCH_DATATEXT_LOGO_\1,\s*\.colors\s*=\s*\{([^}]*)\},?(?:\s*\.colorKeys\s*=\s*([^,]+),?\s*\.colorTitle\s*=\s*([^,]+),?)?\s*\},?\s*#endif'
    
    for match in re.finditer(pattern, content, re.DOTALL):
        name = match.group(1)
        comment = match.group(2).strip()
        names_str = match.group(3)
        colors_str = match.group(4)
        color_keys = match.group(5) if match.group(5) else ''
        color_title = match.group(6) if match.group(6) else ''
        
        names = [n.strip().strip('"') for n in names_str.split(',') if n.strip()]
        colors = []
        for c in colors_str.split(','):
            c = c.strip()
            if not c:
                continue
            # Parse color macro like FF_COLOR_FG_RED
            color_map = {
                'FF_COLOR_FG_RED': '\033[31m',
                'FF_COLOR_FG_GREEN': '\033[32m',
                'FF_COLOR_FG_YELLOW': '\033[33m',
                'FF_COLOR_FG_BLUE': '\033[34m',
                'FF_COLOR_FG_MAGENTA': '\033[35m',
                'FF_COLOR_FG_CYAN': '\033[36m',
                'FF_COLOR_FG_WHITE': '\033[37m',
                'FF_COLOR_FG_BLACK': '\033[30m',
                'FF_COLOR_FG_DEFAULT': '\033[39m',
                'FF_COLOR_FG_LIGHT_RED': '\033[91m',
                'FF_COLOR_FG_LIGHT_GREEN': '\033[92m',
                'FF_COLOR_FG_LIGHT_YELLOW': '\033[93m',
                'FF_COLOR_FG_LIGHT_BLUE': '\033[94m',
                'FF_COLOR_FG_LIGHT_MAGENTA': '\033[95m',
                'FF_COLOR_FG_LIGHT_CYAN': '\033[96m',
                'FF_COLOR_FG_LIGHT_WHITE': '\033[97m',
                'FF_COLOR_FG_LIGHT_BLACK': '\033[90m',
            }
            # Handle FF_COLOR_FG_256 "number"
            if 'FF_COLOR_FG_256' in c:
                num = re.search(r'"(\d+)"', c)
                if num:
                    colors.append(f'\033[38;5;{num.group(1)}m')
                continue
            # Handle FF_COLOR_FG_RGB "r;g;b"
            if 'FF_COLOR_FG_RGB' in c:
                rgb = re.search(r'"([^"]+)"', c)
                if rgb:
                    colors.append(f'\033[38;2;{rgb.group(1)}m')
                continue
            # Handle FF_COLOR_BG_*
            if 'FF_COLOR_BG' in c:
                # Simple background handling
                for k, v in color_map.items():
                    if k.replace('FG', 'BG') in c or k in c:
                        colors.append(v.replace('38', '48').replace('39', '49'))
                        break
                continue
            # Handle FF_COLOR_MODE_BOLD
            if 'FF_COLOR_MODE_BOLD' in c:
                colors.append('\033[1m')
                continue
            # Simple color
            found = False
            for k, v in color_map.items():
                if k in c:
                    colors.append(v)
                    found = True
                    break
            if not found:
                colors.append('\033[39m')
        
        logos.append({
            'name': name,
            'names': names,
            'colors': colors,
            'colorKeys': color_keys,
            'colorTitle': color_title,
        })
    
    return logos

def generate_header(inc_files):
    all_logos = []
    for path in inc_files:
        logos = parse_inc_file(path)
        # Extract logo lines from original inc files
        for logo in logos:
            # Try to find the lines in the inc file
            with open(path, 'r', encoding='utf-8') as f:
                content = f.read()
            pattern = rf'#ifdef FASTFETCH_DATATEXT_LOGO_{logo["name"]}.*?#endif'
            match = re.search(pattern, content, re.DOTALL)
            if match:
                # Extract lines between /* and */ or just lines
                block = match.group(0)
                # Find the actual logo lines (between .lines = and })
                lines_pattern = r'\.lines\s*=\s*FASTFETCH_DATATEXT_LOGO_\w+,\s*//\s*(.+?)(?=\.colors|\.type|#endif|\})'
                lines_match = re.search(lines_pattern, block, re.DOTALL)
                if lines_match:
                    lines_text = lines_match.group(1).strip()
                    logo_lines = [l.strip() for l in lines_text.split('\n') if l.strip()]
                    logo['lines'] = logo_lines
                else:
                    logo['lines'] = [f'${i+1}Logo {logo["name"]} line {i+1}' for i in range(8)]
        all_logos.extend(logos)
    
    # Generate C++ code
    output = []
    output.append('// Auto-generated from .inc files')
    output.append('#include <string>')
    output.append('#include <vector>')
    output.append('#include <unordered_map>')
    output.append('')
    output.append('static const std::unordered_map<std::string, std::vector<LogoEntry>> get_embedded_logos() {')
    output.append('    std::unordered_map<std::string, std::vector<LogoEntry>> logos;')
    output.append('    logos["logos"] = {')
    
    for logo in all_logos:
        output.append('        {')
        output.append(f'            .names = {{')
        names = ', '.join(f'"{n}"' for n in logo['names'])
        output.append(f'                {names}')
        output.append('            },')
        output.append('            .lines = {')
        lines = ',\n'.join(f'                "{l}"' for l in logo['lines'])
        output.append(lines)
        output.append('            },')
        output.append('            .colors = {')
        colors = ',\n'.join(f'                "{c}"' for c in logo['colors'])
        output.append(colors)
        output.append('            },')
        output.append(f'            .colorKeys = "{logo.get("colorKeys", "")}",')
        output.append(f'            .colorTitle = "{logo.get("colorTitle", "")}",')
        output.append('        },')
    
    output.append('    };')
    output.append('    return logos;')
    output.append('}')
    output.append('')
    output.append('static const std::unordered_map<std::string, std::vector<LogoEntry>> LOGOS = get_embedded_logos();')
    
    return '\n'.join(output)

if __name__ == '__main__':
    inc_files = sys.argv[1:] if len(sys.argv) > 1 else [
        'a.inc', 'b.inc', 'c.inc', 'd.inc', 'e.inc', 'f.inc', 'g.inc',
        'h.inc', 'i.inc', 'j.inc', 'k.inc', 'l.inc', 'm.inc', 'n.inc',
        'o.inc', 'p.inc', 'q.inc', 'r.inc', 's.inc', 't.inc', 'u.inc',
        'v.inc', 'w.inc', 'x.inc', 'y.inc', 'z.inc'
    ]
    
    if os.path.exists('generated_logos.hpp'):
        os.remove('generated_logos.hpp')
    
    header = generate_header(inc_files)
    with open('generated_logos.hpp', 'w', encoding='utf-8') as f:
        f.write(header)
    
    print("Generated generated_logos.hpp")
