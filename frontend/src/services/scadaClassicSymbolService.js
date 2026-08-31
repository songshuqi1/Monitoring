const number = (value, fallback = 0) => {
  const parsed = Number(value)
  return Number.isFinite(parsed) ? parsed : fallback
}

const pointsPath = (points) => points
  .map((point, index) => `${index === 0 ? 'M' : 'L'}${point[0]} ${point[1]}`)
  .join('') + 'Z'

const ellipsePath = (cx, cy, rx, ry) => (
  `M${cx - rx} ${cy}A${rx} ${ry} 0 1 0 ${cx + rx} ${cy}A${rx} ${ry} 0 1 0 ${cx - rx} ${cy}Z`
)

const part = (key, d, paint = 'metal', extra = {}) => ({ key, d, paint, ...extra })

export const SCADA_SYMBOL_KINDS = [
  'scada-pipe-h',
  'scada-pipe-v',
  'scada-cylinder-h',
  'scada-cylinder-v',
  'scada-elbow',
  'scada-tee',
  'scada-flange',
  'scada-flow-arrow',
  'scada-heat-coil'
]

export const SCADA_STYLE_OPTIONS = [
  { id: 'none', label: '自定义颜色' },
  { id: 'metal', label: '银灰金属（管道/设备）' },
  { id: 'instrument', label: '青色仪表' },
  { id: 'flow', label: '蓝色流向标识' }
]

export function isScadaSymbol(kind) {
  return SCADA_SYMBOL_KINDS.includes(kind)
}

/**
 * Classic process-control drawing layers. The proportions deliberately match
 * traditional WinCC/SCADA process diagrams: thin black contours, silver pipe
 * surfaces, cyan instruments and solid-blue flow arrows.
 */
export function scadaSymbolParts(shape) {
  const x = number(shape?.x)
  const y = number(shape?.y)
  const w = Math.max(16, number(shape?.w, 96))
  const h = Math.max(16, number(shape?.h, 54))
  const line = Math.max(0.9, Math.min(2, number(shape?.strokeWidth, 1.2)))

  if (shape?.kind === 'scada-pipe-h') {
    const top = y + h * 0.25
    const bottom = y + h * 0.75
    const cy = (top + bottom) / 2
    const rx = Math.max(2.5, Math.min(w * 0.1, (bottom - top) * 0.28))
    const leftCx = x + rx
    const rightCx = x + w - rx
    return [
      part('body', pointsPath([[leftCx, top], [rightCx, top], [rightCx, bottom], [leftCx, bottom]]), 'metal', { stroke: 'none' }),
      part('left-end', ellipsePath(leftCx, cy, rx, (bottom - top) / 2), 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('right-end', ellipsePath(rightCx, cy, rx, (bottom - top) / 2), 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('top-outline', `M${leftCx} ${top}H${rightCx}`, 'none', { strokePaint: 'outline', strokeWidth: line }),
      part('bottom-outline', `M${leftCx} ${bottom}H${rightCx}`, 'none', { strokePaint: 'outline', strokeWidth: line }),
      part('top-glint', `M${leftCx + rx * 0.35} ${top + h * 0.075}H${rightCx - rx * 0.35}`, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(0.7, line * 0.72), strokeOpacity: 0.95 }),
      part('bottom-shade', `M${leftCx + rx * 0.3} ${bottom - h * 0.065}H${rightCx - rx * 0.3}`, 'none', { strokePaint: 'shade', strokeWidth: Math.max(0.7, line * 0.72), strokeOpacity: 0.8 })
    ]
  }

  if (shape?.kind === 'scada-pipe-v') {
    const left = x + w * 0.25
    const right = x + w * 0.75
    const cx = (left + right) / 2
    const ry = Math.max(2.5, Math.min(h * 0.1, (right - left) * 0.28))
    const topCy = y + ry
    const bottomCy = y + h - ry
    return [
      part('body', pointsPath([[left, topCy], [right, topCy], [right, bottomCy], [left, bottomCy]]), 'metal-v', { stroke: 'none' }),
      part('top-end', ellipsePath(cx, topCy, (right - left) / 2, ry), 'metal', { strokePaint: 'outline', strokeWidth: line }),
      part('bottom-end', ellipsePath(cx, bottomCy, (right - left) / 2, ry), 'metal', { strokePaint: 'outline', strokeWidth: line }),
      part('left-outline', `M${left} ${topCy}V${bottomCy}`, 'none', { strokePaint: 'outline', strokeWidth: line }),
      part('right-outline', `M${right} ${topCy}V${bottomCy}`, 'none', { strokePaint: 'outline', strokeWidth: line }),
      part('left-glint', `M${left + w * 0.075} ${topCy + ry * 0.35}V${bottomCy - ry * 0.35}`, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(0.7, line * 0.72), strokeOpacity: 0.9 }),
      part('right-shade', `M${right - w * 0.065} ${topCy + ry * 0.3}V${bottomCy - ry * 0.3}`, 'none', { strokePaint: 'shade', strokeWidth: Math.max(0.7, line * 0.72), strokeOpacity: 0.8 })
    ]
  }

  if (shape?.kind === 'scada-cylinder-h') {
    const shellTop = y + h * 0.22
    const shellBottom = y + h * 0.74
    const cy = (shellTop + shellBottom) / 2
    const rx = Math.max(5, Math.min(w * 0.13, (shellBottom - shellTop) * 0.48))
    const leftCx = x + w * 0.18
    const rightCx = x + w * 0.82
    const footTop = y + h * 0.76
    return [
      part('shadow', ellipsePath(x + w * 0.52, y + h * 0.91, w * 0.33, h * 0.045), 'shade', { fillOpacity: 0.22, stroke: 'none' }),
      part('body', pointsPath([[leftCx, shellTop], [rightCx, shellTop], [rightCx, shellBottom], [leftCx, shellBottom]]), 'metal', { stroke: 'none' }),
      part('left-head', ellipsePath(leftCx, cy, rx, (shellBottom - shellTop) / 2), 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('right-head', ellipsePath(rightCx, cy, rx, (shellBottom - shellTop) / 2), 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('shell-top', `M${leftCx} ${shellTop}H${rightCx}`, 'none', { strokePaint: 'outline', strokeWidth: line }),
      part('shell-bottom', `M${leftCx} ${shellBottom}H${rightCx}`, 'none', { strokePaint: 'outline', strokeWidth: line }),
      part('shell-glint', `M${leftCx + rx * 0.3} ${shellTop + h * 0.08}H${rightCx - rx * 0.3}`, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(0.8, line * 0.78) }),
      part('foot-left', pointsPath([[x + w * 0.28, footTop], [x + w * 0.4, footTop], [x + w * 0.44, y + h * 0.88], [x + w * 0.24, y + h * 0.88]]), 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('foot-right', pointsPath([[x + w * 0.61, footTop], [x + w * 0.73, footTop], [x + w * 0.77, y + h * 0.88], [x + w * 0.57, y + h * 0.88]]), 'metal-v', { strokePaint: 'outline', strokeWidth: line })
    ]
  }

  if (shape?.kind === 'scada-cylinder-v') {
    const cx = x + w * 0.5
    const rx = w * 0.3
    const ry = Math.max(3, h * 0.07)
    const topCy = y + h * 0.16
    const bottomCy = y + h * 0.76
    return [
      part('shadow', ellipsePath(cx + w * 0.035, y + h * 0.91, w * 0.34, h * 0.04), 'shade', { fillOpacity: 0.22, stroke: 'none' }),
      part('body', `M${cx - rx} ${topCy}L${cx - rx} ${bottomCy}A${rx} ${ry} 0 0 0 ${cx + rx} ${bottomCy}L${cx + rx} ${topCy}A${rx} ${ry} 0 0 1 ${cx - rx} ${topCy}Z`, 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('top-head', ellipsePath(cx, topCy, rx, ry), 'metal', { strokePaint: 'outline', strokeWidth: line }),
      part('bottom-head', ellipsePath(cx, bottomCy, rx, ry), 'metal', { strokePaint: 'outline', strokeWidth: line }),
      part('body-glint', `M${cx - rx * 0.55} ${topCy + ry * 0.8}V${bottomCy - ry * 0.9}`, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(0.8, line * 0.8), strokeOpacity: 0.92 }),
      part('base', pointsPath([[cx - rx * 1.22, y + h * 0.81], [cx + rx * 1.22, y + h * 0.81], [cx + rx * 1.1, y + h * 0.89], [cx - rx * 1.1, y + h * 0.89]]), 'metal-v', { strokePaint: 'outline', strokeWidth: line })
    ]
  }

  if (shape?.kind === 'scada-elbow') {
    const width = Math.max(7, Math.min(w, h) * 0.25)
    const route = `M${x + w * 0.18} ${y + h * 0.77}H${x + w * 0.68}Q${x + w * 0.82} ${y + h * 0.77} ${x + w * 0.82} ${y + h * 0.63}V${y + h * 0.18}`
    return [
      part('outline', route, 'none', { strokePaint: 'outline', strokeWidth: width + line * 2, strokeLinecap: 'butt', strokeLinejoin: 'round' }),
      part('pipe', route, 'none', { strokePaint: 'metal', strokeWidth: width, strokeLinecap: 'butt', strokeLinejoin: 'round' }),
      part('glint', route, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(1, width * 0.16), strokeOpacity: 0.9, transform: `translate(${-width * 0.12} ${-width * 0.12})`, strokeLinecap: 'butt', strokeLinejoin: 'round' })
    ]
  }

  if (shape?.kind === 'scada-tee') {
    const width = Math.max(7, Math.min(w, h) * 0.22)
    const route = `M${x + w * 0.1} ${y + h * 0.38}H${x + w * 0.9}M${x + w * 0.56} ${y + h * 0.38}V${y + h * 0.85}`
    return [
      part('outline', route, 'none', { strokePaint: 'outline', strokeWidth: width + line * 2, strokeLinecap: 'butt', strokeLinejoin: 'miter' }),
      part('pipe', route, 'none', { strokePaint: 'metal', strokeWidth: width, strokeLinecap: 'butt', strokeLinejoin: 'miter' }),
      part('glint', `M${x + w * 0.1} ${y + h * 0.33}H${x + w * 0.9}M${x + w * 0.51} ${y + h * 0.38}V${y + h * 0.85}`, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(1, width * 0.14), strokeOpacity: 0.85, strokeLinecap: 'butt' })
    ]
  }

  if (shape?.kind === 'scada-flange') {
    const top = y + h * 0.18
    const bottom = y + h * 0.82
    const innerLeft = x + w * 0.25
    const innerRight = x + w * 0.75
    const band = Math.max(3, w * 0.1)
    return [
      part('pipe', pointsPath([[x, y + h * 0.35], [x + w, y + h * 0.35], [x + w, y + h * 0.65], [x, y + h * 0.65]]), 'metal', { strokePaint: 'outline', strokeWidth: line }),
      part('left-flange', pointsPath([[innerLeft - band, top], [innerLeft + band, top], [innerLeft + band, bottom], [innerLeft - band, bottom]]), 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('right-flange', pointsPath([[innerRight - band, top], [innerRight + band, top], [innerRight + band, bottom], [innerRight - band, bottom]]), 'metal-v', { strokePaint: 'outline', strokeWidth: line }),
      part('left-glint', `M${innerLeft - band * 0.48} ${top + line}V${bottom - line}`, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(0.7, line * 0.7) }),
      part('right-glint', `M${innerRight - band * 0.48} ${top + line}V${bottom - line}`, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(0.7, line * 0.7) })
    ]
  }

  if (shape?.kind === 'scada-flow-arrow') {
    const cy = y + h * 0.5
    return [
      part('arrow', pointsPath([[x + w * 0.08, cy - h * 0.12], [x + w * 0.64, cy - h * 0.12], [x + w * 0.64, y + h * 0.23], [x + w * 0.94, cy], [x + w * 0.64, y + h * 0.77], [x + w * 0.64, cy + h * 0.12], [x + w * 0.08, cy + h * 0.12]]), 'blue', { stroke: 'none', strokeWidth: 0 })
    ]
  }

  if (shape?.kind === 'scada-heat-coil') {
    const left = x + w * 0.16
    const right = x + w * 0.84
    const top = y + h * 0.14
    const step = h * 0.2
    const coil = `M${left} ${top}H${right - w * 0.08}Q${right} ${top} ${right} ${top + step * 0.5}Q${right} ${top + step} ${right - w * 0.08} ${top + step}H${left + w * 0.08}Q${x} ${top + step} ${x} ${top + step * 1.5}Q${x} ${top + step * 2} ${left + w * 0.08} ${top + step * 2}H${right - w * 0.08}Q${right} ${top + step * 2} ${right} ${top + step * 2.5}Q${right} ${top + step * 3} ${right - w * 0.08} ${top + step * 3}H${left}`
    return [
      part('coil-outline', coil, 'none', { strokePaint: 'outline', strokeWidth: Math.max(4, Math.min(w, h) * 0.075), strokeLinecap: 'round', strokeLinejoin: 'round' }),
      part('coil', coil, 'none', { strokePaint: 'cyan', strokeWidth: Math.max(2, Math.min(w, h) * 0.04), strokeLinecap: 'round', strokeLinejoin: 'round' }),
      part('coil-glint', coil, 'none', { strokePaint: 'highlight', strokeWidth: Math.max(0.65, Math.min(w, h) * 0.012), strokeOpacity: 0.72, transform: 'translate(0 -0.7)', strokeLinecap: 'round', strokeLinejoin: 'round' })
    ]
  }

  return [part('fallback', ellipsePath(x + w / 2, y + h / 2, w * 0.25, h * 0.25), 'metal', { strokePaint: 'outline', strokeWidth: line })]
}
