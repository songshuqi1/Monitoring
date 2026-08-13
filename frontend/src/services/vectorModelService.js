const DEFAULT_BOUNDS = { l: 0, r: 0, t: 0, b: 0 }

const VECTOR_MODELS = {
  processPump: {
    shapeType: 'customPath',
    bounds: { l: 0.04, r: 0.04, t: 0.08, b: 0.05 },
    ports: {
      left: { x: 0.04, y: 0.53125 },
      right: { x: 0.96, y: 0.53125 },
      top: { x: 0.5, y: 0.08 },
      bottom: { x: 0.5, y: 0.95 }
    }
  },
  processPool: {
    shapeType: 'customPath',
    bounds: { l: 0.04, r: 0.04, t: 0.12, b: 0.12 },
    ports: {
      left: { x: 0.04, y: 0.5 },
      right: { x: 0.96, y: 0.5 },
      top: { y: 0.12, edge: 'top', dynamicAxis: 'x', min: 0.04, max: 0.96 },
      bottom: { y: 0.88, edge: 'bottom', dynamicAxis: 'x', min: 0.04, max: 0.96 }
    }
  },
  processDam: {
    shapeType: 'customPath',
    bounds: { l: 0.01, r: 0.03, t: 0.12, b: 0.2 },
    ports: {
      left: { x: 0.01, y: 0.52 },
      right: { x: 0.97, y: 0.52 },
      top: { y: 0.12, edge: 'top', dynamicAxis: 'x', min: 0.01, max: 0.97 },
      bottom: { y: 0.8, edge: 'bottom', dynamicAxis: 'x', min: 0.01, max: 0.97 }
    }
  },
  processTank: {
    shapeType: 'customPath',
    bounds: { l: 0.24, r: 0.24, t: 0, b: 0 },
    ports: {
      top: { x: 0.5, y: 0 },
      bottom: { x: 0.5, y: 1 },
      left: { x: 0.24, y: 0.5 },
      right: { x: 0.76, y: 0.5 }
    }
  },
  processMixer: {
    shapeType: 'customPath',
    bounds: { l: 0.12, r: 0.12, t: 0.16, b: 0.05 },
    ports: {
      left: { x: 0.13, y: 0.38 },
      right: { x: 0.87, y: 0.62 },
      top: { x: 0.5, y: 0.16 },
      bottom: { x: 0.5, y: 0.95 }
    }
  },
  processDosingMachine: {
    shapeType: 'customPath',
    bounds: { l: 0, r: 0, t: 0.1, b: 0.08 },
    ports: {
      left: { x: 0, y: 0.52 },
      right: { x: 1, y: 0.52 },
      top: { x: 0.5, y: 0.1 },
      bottom: { x: 0.5, y: 0.92 }
    }
  },
  processJunction: {
    shapeType: 'circle',
    bounds: { l: 0, r: 0, t: 0, b: 0 },
    ports: {
      left: { x: 0, y: 0.5 },
      right: { x: 1, y: 0.5 },
      top: { x: 0.5, y: 0 },
      bottom: { x: 0.5, y: 1 }
    }
  },
  processValve: {
    shapeType: 'customPath',
    bounds: { l: 0, r: 0, t: 0.08, b: 0.08 },
    ports: {
      left: { x: 0, y: 0.5 },
      right: { x: 1, y: 0.5 },
      top: { x: 0.5, y: 0.08 },
      bottom: { x: 0.5, y: 0.92 }
    }
  },
  processValveVertical: {
    shapeType: 'customPath',
    bounds: { l: 0.08, r: 0.08, t: 0, b: 0 },
    ports: {
      top: { x: 0.5, y: 0 },
      bottom: { x: 0.5, y: 1 },
      left: { x: 0.08, y: 0.5 },
      right: { x: 0.92, y: 0.5 }
    }
  },
  processPipeEnd: {
    shapeType: 'customPath',
    bounds: { l: 0, r: 0.04, t: 0.2, b: 0.2 },
    ports: {
      left: { x: 0.04, y: 0.5 },
      right: { x: 0.96, y: 0.5 },
      top: { x: 0.5, y: 0.2 },
      bottom: { x: 0.5, y: 0.8 }
    }
  },
  processBagFilter: {
    shapeType: 'customPath',
    bounds: { l: 0, r: 0, t: 16 / 132, b: 0 },
    ports: {
      left: { x: 0, y: 36 / 132 },
      right: { x: 1, y: 36 / 132 },
      top: { x: 0.5, y: 16 / 132 },
      bottom: { x: 0.5, y: 1 }
    }
  },
  processFanBlower: {
    shapeType: 'customPath',
    bounds: { l: 0, r: 0, t: 0, b: 0.06 },
    ports: {
      left: { x: 0, y: 54 / 104 },
      right: { x: 1, y: 54 / 104 },
      top: { x: 62 / 122, y: 0 }
    }
  },
  processWinccFan: {
    shapeType: 'circle',
    bounds: { l: 0.2, r: 0.2, t: 0, b: 0.08 },
    ports: {
      top: { x: 0.5, y: 0 }
    }
  },
  processRotaryAirlock: {
    shapeType: 'circle',
    bounds: { l: 0, r: 0, t: 0, b: 0 },
    ports: {
      top: { x: 0.5, y: 0 }
    }
  },
  processHeatExchanger: {
    shapeType: 'customPath',
    bounds: { l: 20 / 86, r: 20 / 86, t: 0, b: 0 },
    ports: {
      top: { x: 0.5, y: 0 },
      bottom: { x: 0.5, y: 1 }
    }
  },
  processValueTag: {
    shapeType: 'label',
    bounds: DEFAULT_BOUNDS,
    ports: {}
  }
}

function modelDefinition(widget) {
  const configuredPorts = configuredPortMap(widget)
  if (configuredPorts) {
    return {
      shapeType: widget?.type === 'customShape' ? 'customShape' : 'customPath',
      bounds: DEFAULT_BOUNDS,
      ports: configuredPorts
    }
  }
  return VECTOR_MODELS[widget?.type] || {
    shapeType: 'rect',
    bounds: DEFAULT_BOUNDS,
    ports: {}
  }
}

function configuredPortMap(widget) {
  const ports = Array.isArray(widget?.config?.connectionPorts)
    ? widget.config.connectionPorts
    : []
  if (!ports.length) return null
  const map = {}
  ports.forEach(port => {
    const side = ['top', 'right', 'bottom', 'left'].includes(port.side) ? port.side : null
    if (!side || map[side]) return
    const x = Number(port.x)
    const y = Number(port.y)
    if (!Number.isFinite(x) || !Number.isFinite(y)) return
    map[side] = {
      x: Math.max(0, Math.min(1, x)),
      y: Math.max(0, Math.min(1, y))
    }
  })
  return Object.keys(map).length ? map : null
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value))
}

export function vectorBounds(widget, pad = 0) {
  if (!widget) return { x: 0, y: 0, w: 0, h: 0 }
  const inset = modelDefinition(widget).bounds || DEFAULT_BOUNDS
  const left = widget.x + widget.w * inset.l
  const top = widget.y + widget.h * inset.t
  const right = widget.x + widget.w * (1 - inset.r)
  const bottom = widget.y + widget.h * (1 - inset.b)
  return {
    x: left - pad,
    y: top - pad,
    w: Math.max(0, right - left + pad * 2),
    h: Math.max(0, bottom - top + pad * 2)
  }
}

export function vectorCenter(widget) {
  const b = vectorBounds(widget)
  return {
    x: b.x + b.w / 2,
    y: b.y + b.h / 2
  }
}

function defaultPort(widget, portName) {
  const center = { x: widget.x + widget.w / 2, y: widget.y + widget.h / 2 }
  const ports = {
    left: { x: widget.x, y: center.y },
    right: { x: widget.x + widget.w, y: center.y },
    top: { x: center.x, y: widget.y },
    bottom: { x: center.x, y: widget.y + widget.h },
    center
  }
  return ports[portName] || null
}

function normalizedPumpPort(widget, portName) {
  if (widget?.type !== 'processPump') return portName
  if (portName === 'top') return widget?.config?.flipX ? 'right' : 'left'
  if (portName === 'bottom') return widget?.config?.flipX ? 'left' : 'right'
  return portName
}

function resolvePort(widget, portName, target = null) {
  if (!widget || !portName) return null
  const resolvedName = normalizedPumpPort(widget, portName)
  const definition = modelDefinition(widget)
  const port = definition.ports?.[resolvedName]
  if (!port) return defaultPort(widget, resolvedName)

  if (port.dynamicAxis === 'x') {
    const targetCenter = target ? vectorCenter(target) : vectorCenter(widget)
    const minX = widget.x + widget.w * (port.min ?? 0)
    const maxX = widget.x + widget.w * (port.max ?? 1)
    return {
      x: clamp(targetCenter.x, minX, maxX),
      y: widget.y + widget.h * port.y
    }
  }

  if (port.dynamicAxis === 'y') {
    const targetCenter = target ? vectorCenter(target) : vectorCenter(widget)
    const minY = widget.y + widget.h * (port.min ?? 0)
    const maxY = widget.y + widget.h * (port.max ?? 1)
    return {
      x: widget.x + widget.w * port.x,
      y: clamp(targetCenter.y, minY, maxY)
    }
  }

  return {
    x: widget.x + widget.w * port.x,
    y: widget.y + widget.h * port.y
  }
}

export function vectorPort(widget, portName, target = null) {
  return resolvePort(widget, portName, target)
}

export function vectorAnchor(widget, target, portName = null) {
  const namedPort = resolvePort(widget, portName, target)
  if (namedPort) return namedPort

  const from = vectorBounds(widget)
  const to = vectorBounds(target)
  const sx = from.x + from.w / 2
  const sy = from.y + from.h / 2
  const tx = to.x + to.w / 2
  const ty = to.y + to.h / 2
  const dx = tx - sx
  const dy = ty - sy
  const side = Math.abs(dx) >= Math.abs(dy)
    ? (dx >= 0 ? 'right' : 'left')
    : (dy >= 0 ? 'bottom' : 'top')
  return resolvePort(widget, side, target) || defaultPort(widget, side)
}

export function vectorObstacleBounds(widget, pad = 0) {
  return vectorBounds(widget, pad)
}

export function createVectorModel(widget) {
  const definition = modelDefinition(widget)
  const bounds = vectorBounds(widget)
  const center = vectorCenter(widget)
  const anchors = {}
  Object.keys(definition.ports || {}).forEach(name => {
    anchors[name] = resolvePort(widget, name)
  })
  return {
    id: widget.id,
    type: widget.type,
    shapeType: definition.shapeType || 'rect',
    centerX: center.x,
    centerY: center.y,
    width: widget.w,
    height: widget.h,
    bounds,
    ports: Object.entries(anchors).map(([name, point]) => ({ name, ...point })),
    anchors
  }
}
