---
description: UI positioning guidelines for VCV Rack modules
---

# UI Positioning Guidelines

## IMPORTANT - Coordinate System

- **NEVER use `mm2px()`** - The user has Inkscape configured in pixels, not millimeters
- **Always use `Vec(x, y)` directly** with pixel coordinates
- All existing positions in the codebase use pixels (e.g., `Vec(15.0, 82.0)`)

## Examples

### ❌ INCORRECT
```cpp
addParam(createParamCentered<CKSS>(mm2px(Vec(15.0, 82.0)), module, ...));
```

### ✅ CORRECT
```cpp
addParam(createParamCentered<CKSS>((Vec(15.0, 82.0)), module, ...));
```

## Reference Values

- Panel SVG is designed in pixels in Inkscape
- Knobs, jacks, and all UI elements use pixel coordinates
- Default VCV Rack grid: 1 HP = 5.08mm, but positions are in pixels

## When Adding New Elements

1. Check existing positions in the same file for reference
2. Use `Vec(x, y)` directly
3. Do NOT convert from mm to pixels
