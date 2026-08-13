function widget(id, type, x, y, w, h, config = {}) {
  return { id, type, x, y, w, h, config }
}

function label(id, text, x, y, w, h, config = {}) {
  return widget(id, 'label', x, y, w, h, {
    text,
    fontSize: 14,
    color: '#000000',
    bold: false,
    align: 'center',
    verticalAlign: 'center',
    background: 'transparent',
    borderColor: 'transparent',
    borderWidth: 0,
    borderRadius: 0,
    ...config
  })
}

function captionLabel(id, text, x, y, w, h, config = {}) {
  return label(id, text, x, y, w, h, {
    fontSize: 12,
    bold: true,
    background: 'transparent',
    borderColor: 'transparent',
    borderWidth: 0,
    borderRadius: 0,
    ...config
  })
}

function navButton(id, text, x, active = false) {
  return label(id, text, x, 1026, 102, 31, {
    fontSize: 14,
    bold: true,
    color: '#111111',
    background: active ? '#00ff33' : '#f7f7f7',
    borderColor: '#8a8a8a',
    borderWidth: 1,
    borderRadius: 0
  })
}

function tag(id, x, y, w, h, config = {}) {
  return widget(id, 'processValueTag', x, y, w, h, {
    label: '',
    decimals: 1,
    valueFontSize: 16,
    titleFontSize: 11,
    ...config
  })
}

function metricTag(id, x, y, unit, options = {}) {
  const {
    w = 64,
    h = 28,
    valueFontSize = 15,
    titleFontSize = 10,
    ...config
  } = options
  return tag(id, x, y, w, h, {
    unit,
    valueFontSize,
    titleFontSize,
    ...config
  })
}

function line(id, from, to, route = {}) {
  return {
    id,
    from,
    to,
    color: route.color || '#176e45',
    width: route.width || 2,
    kind: 'pipe',
    manualRoute: Array.isArray(route.waypoints) || Boolean(route.manualRoute),
    ...route,
    waypoints: Array.isArray(route.waypoints)
      ? route.waypoints.map(([x, y]) => ({ x, y }))
      : route.waypoints
  }
}

function roundCoord(value) {
  return Math.round(Number(value) || 0)
}

function scaleWidgetConfig(config = {}, scaleX = 1, scaleY = 1) {
  const next = { ...config }
  if (Number.isFinite(Number(next.labelOffsetX))) next.labelOffsetX = roundCoord(Number(next.labelOffsetX) * scaleX)
  if (Number.isFinite(Number(next.labelOffsetY))) next.labelOffsetY = roundCoord(Number(next.labelOffsetY) * scaleY)
  return next
}

function scaleWidgetGeometry(item, scaleX = 1, scaleY = 1, offsetX = 0, offsetY = 0) {
  return {
    ...item,
    x: roundCoord(item.x * scaleX + offsetX),
    y: roundCoord(item.y * scaleY + offsetY),
    w: Math.max(1, roundCoord(item.w * scaleX)),
    h: Math.max(1, roundCoord(item.h * scaleY)),
    config: scaleWidgetConfig(item.config || {}, scaleX, scaleY)
  }
}

function scalePoint(point, scaleX = 1, scaleY = 1, offsetX = 0, offsetY = 0) {
  return {
    x: roundCoord((Number(point.x) || 0) * scaleX + offsetX),
    y: roundCoord((Number(point.y) || 0) * scaleY + offsetY)
  }
}

function scaleConnectionGeometry(conn, scaleX = 1, scaleY = 1, offsetX = 0, offsetY = 0) {
  return {
    ...conn,
    waypoints: Array.isArray(conn.waypoints)
      ? conn.waypoints.map(point => scalePoint(point, scaleX, scaleY, offsetX, offsetY))
      : conn.waypoints,
    points: Array.isArray(conn.points)
      ? conn.points.map(point => scalePoint(point, scaleX, scaleY, offsetX, offsetY))
      : conn.points,
    bendX: Number.isFinite(Number(conn.bendX)) ? roundCoord(Number(conn.bendX) * scaleX + offsetX) : conn.bendX,
    bendY: Number.isFinite(Number(conn.bendY)) ? roundCoord(Number(conn.bendY) * scaleY + offsetY) : conn.bendY
  }
}

function scaleTemplateLayout(layout, options = {}) {
  const {
    scaleX = 1,
    scaleY = scaleX,
    offsetX = 0,
    offsetY = 0
  } = options

  return {
    widgets: layout.widgets.map(item => scaleWidgetGeometry(item, scaleX, scaleY, offsetX, offsetY)),
    connections: layout.connections.map(item => scaleConnectionGeometry(item, scaleX, scaleY, offsetX, offsetY))
  }
}

function patchWidget(widgetMap, id, patch = {}) {
  const item = widgetMap.get(id)
  if (!item) return
  const { config, ...rest } = patch
  Object.assign(item, rest)
  if (config) item.config = { ...(item.config || {}), ...config }
}

function placeAbove(widgetMap, id, anchorId, options = {}) {
  const anchor = widgetMap.get(anchorId)
  const item = widgetMap.get(id)
  if (!anchor || !item) return
  const {
    gap = 18,
    dx = 0,
    dy = 0,
    w = item.w,
    h = item.h,
    config = {}
  } = options
  patchWidget(widgetMap, id, {
    x: roundCoord(anchor.x + anchor.w / 2 - w / 2 + dx),
    y: roundCoord(anchor.y - h - gap + dy),
    w,
    h,
    config
  })
}

function placeBelow(widgetMap, id, anchorId, options = {}) {
  const anchor = widgetMap.get(anchorId)
  const item = widgetMap.get(id)
  if (!anchor || !item) return
  const {
    gap = 16,
    dx = 0,
    dy = 0,
    w = item.w,
    h = item.h,
    config = {}
  } = options
  patchWidget(widgetMap, id, {
    x: roundCoord(anchor.x + anchor.w / 2 - w / 2 + dx),
    y: roundCoord(anchor.y + anchor.h + gap + dy),
    w,
    h,
    config
  })
}

function placeRight(widgetMap, id, anchorId, options = {}) {
  const anchor = widgetMap.get(anchorId)
  const item = widgetMap.get(id)
  if (!anchor || !item) return
  const {
    gap = 18,
    dx = 0,
    dy = 0,
    w = item.w,
    h = item.h,
    config = {}
  } = options
  patchWidget(widgetMap, id, {
    x: roundCoord(anchor.x + anchor.w + gap + dx),
    y: roundCoord(anchor.y + anchor.h / 2 - h / 2 + dy),
    w,
    h,
    config
  })
}

function pumpSet(id, x, y, name, tagId, options = {}) {
  const {
    pumpW = 62,
    pumpH = 56,
    labelX = x - 22,
    labelY = y + 58,
    labelW = 112,
    labelH = 22,
    labelFontSize = 13,
    labelAlign = 'center',
    tagX = x + 16,
    tagY = y - 24,
    tagW = 64,
    tagH = 28,
    tagUnit = 'Hz'
  } = options
  return [
    widget(id, 'processPump', x, y, pumpW, pumpH, { label: '' }),
    label(`${id}-txt`, name, labelX, labelY, labelW, labelH, {
      fontSize: labelFontSize,
      bold: true,
      color: '#111111',
      align: labelAlign,
      background: 'transparent',
      borderColor: 'transparent',
      borderWidth: 0
    }),
    metricTag(tagId, tagX, tagY, tagUnit, { w: tagW, h: tagH })
  ]
}

function tankSet(id, x, pumpId, pumpName, tagId, options = {}) {
  const {
    tankY = 518,
    tankW = 58,
    tankH = 104,
    pumpX = x - 4,
    pumpY = 622,
    pumpW = 62,
    pumpH = 56,
    labelX = x - 22,
    labelY = 682,
    labelW = 110,
    labelH = 22,
    labelFontSize = 13,
    tagX = x + 30,
    tagY = 590,
    tagW = 60,
    tagH = 28,
    tagUnit = 'Hz'
  } = options
  return [
    widget(id, 'processTank', x, tankY, tankW, tankH, { label: '' }),
    widget(pumpId, 'processPump', pumpX, pumpY, pumpW, pumpH, { label: '' }),
    label(`${pumpId}-txt`, pumpName, labelX, labelY, labelW, labelH, {
      fontSize: labelFontSize
      ,
      bold: true,
      color: '#111111',
      background: 'transparent',
      borderColor: 'transparent',
      borderWidth: 0
    }),
    metricTag(tagId, tagX, tagY, tagUnit, { w: tagW, h: tagH })
  ]
}

export function createTailingsWaterTemplate() {
  const widgets = [
    // 顶部状态区
    label('tpl-logo', 'T', 28, 44, 58, 58, {
      fontSize: 30,
      bold: true,
      color: '#0ea5e9',
      background: '#f8fafc',
      borderColor: '#e5e7eb',
      borderWidth: 1,
      borderRadius: 40
    }),
    label('tpl-title', '尾矿供水监控画面', 884, 33, 260, 34, {
      fontSize: 25,
      color: '#0000cc',
      bold: true
    }),
    label('tpl-top-confirm', '报警确认', 1288, 8, 98, 24, { fontSize: 13, background: '#ffffff', borderColor: '#c7c7c7', borderWidth: 1 }),
    label('tpl-top-op', '操作员', 1390, 8, 102, 24, { fontSize: 13, background: '#ffffff', borderColor: '#c7c7c7', borderWidth: 1 }),
    label('tpl-top-value', '0.000', 1548, 8, 96, 24, { fontSize: 18, background: '#ffffff' }),
    label('tpl-top-time', '2026/6/22  9:12:00', 1656, 8, 214, 24, { fontSize: 18, background: '#ffffff' }),

    // 左上冲洗水和尾矿泵区
    captionLabel('txt-flush-main', '冲洗水总管', 26, 180, 118, 22, { align: 'left' }),
    captionLabel('txt-flush-valve', '冲洗水总管闸板阀', 162, 180, 172, 22),
    widget('tpl-in-left', 'processPipeEnd', 4, 160, 56, 22, { label: '' }),
    widget('tpl-v-main', 'processValve', 278, 154, 42, 34, { label: '' }),
    metricTag('tpl-tag-flush', 156, 220, 'm3/h', { w: 76 }),
    metricTag('tpl-tag-left-flow', 126, 300, 'KPa', { w: 70 }),
    metricTag('tpl-tag-left-bottom', 292, 754, 'KPa', { w: 72 }),

    widget('tpl-pump-1', 'processPump', 536, 216, 72, 62, { label: '' }),
    widget('tpl-pump-2', 'processPump', 584, 302, 72, 62, { label: '' }),
    captionLabel('txt-pump-1', '1号总尾矿渣浆泵', 470, 286, 168, 20),
    captionLabel('txt-pump-2', '2号总尾矿渣浆泵', 518, 372, 168, 20),
    metricTag('tpl-tag-pump-1', 510, 182, 'Hz', { w: 58 }),
    metricTag('tpl-tag-pump-2', 620, 268, 'Hz', { w: 58 }),
    widget('tpl-v-12', 'processValve', 742, 214, 42, 35, { label: '' }),
    widget('tpl-v-11', 'processValve', 972, 214, 42, 35, { label: '' }),
    widget('tpl-v-21', 'processValve', 742, 301, 42, 35, { label: '' }),
    widget('tpl-v-22', 'processValve', 972, 301, 42, 35, { label: '' }),
    captionLabel('txt-v-12', '1-2闸板阀', 698, 254, 124, 20),
    captionLabel('txt-v-11', '1-1闸板阀', 928, 254, 124, 20),
    captionLabel('txt-v-21', '2-1闸板阀', 698, 340, 124, 20),
    captionLabel('txt-v-22', '2-2闸板阀', 928, 340, 124, 20),

    // 尾矿库、水罐、回水泵
    captionLabel('txt-dam', '尾矿库', 80, 338, 72, 22, { align: 'left' }),
    widget('tpl-dam', 'processDam', 67, 365, 545, 118, { label: '' }),
    captionLabel('txt-tank', '水罐', 44, 548, 54, 22, { align: 'left' }),
    ...tankSet('tpl-tank-5', 98, 'tpl-return-5', '5号回水泵', 'tpl-tag-r5', {
      labelX: 64,
      tagX: 129,
      labelFontSize: 12
    }),
    ...tankSet('tpl-tank-6', 190, 'tpl-return-6', '6号回水泵', 'tpl-tag-r6', {
      labelX: 156,
      tagX: 221,
      labelFontSize: 12
    }),
    ...tankSet('tpl-tank-1', 282, 'tpl-return-1', '1号回水泵', 'tpl-tag-r1', {
      labelX: 248,
      tagX: 313,
      labelFontSize: 12
    }),
    ...tankSet('tpl-tank-2', 374, 'tpl-return-2', '2号回水泵', 'tpl-tag-r2', {
      labelX: 340,
      tagX: 405,
      labelFontSize: 12
    }),
    ...tankSet('tpl-tank-3', 466, 'tpl-return-3', '3号回水泵', 'tpl-tag-r3', {
      labelX: 432,
      tagX: 497,
      labelFontSize: 12
    }),
    ...tankSet('tpl-tank-4', 558, 'tpl-return-4', '4号回水泵', 'tpl-tag-r4', {
      labelX: 524,
      tagX: 589,
      labelFontSize: 12
    }),
    widget('tpl-out-left', 'processPipeEnd', 3, 715, 56, 22, { label: '' }),
    metricTag('tpl-tag-bottom-left', 34, 736, 'm3/h', { w: 74 }),

    // 中部水池
    captionLabel('txt-prod-pool', '生产水池', 720, 548, 96, 22, { align: 'left' }),
    widget('tpl-pool-prod', 'processPool', 716, 598, 690, 138, { label: '', fill: 60 }),
    metricTag('tpl-tag-prod-level', 750, 606, 'm', { w: 62 }),
    captionLabel('txt-cold-pool', '冷却水池', 1582, 548, 104, 22, { align: 'left' }),
    widget('tpl-pool-cold', 'processPool', 1428, 598, 260, 138, { label: '', fill: 62 }),
    metricTag('tpl-tag-cold-level', 1600, 606, 'm', { w: 62 }),
    captionLabel('txt-acc-pool', '事故泵池', 1738, 548, 96, 22, { align: 'left' }),
    widget('tpl-pool-accident', 'processPool', 1710, 598, 140, 138, { label: '', fill: 55 }),
    metricTag('tpl-tag-acc-level', 1746, 576, 'm', { w: 64 }),

    ...pumpSet('tpl-loop-4', 796, 788, '4号浊环水泵', 'tpl-tag-loop-4', {
      labelX: 742, labelW: 92, labelFontSize: 12, tagX: 812
    }),
    ...pumpSet('tpl-loop-3', 982, 788, '3号浊环水泵', 'tpl-tag-loop-3', {
      labelX: 928, labelW: 92, labelFontSize: 12, tagX: 998
    }),
    ...pumpSet('tpl-loop-2', 1168, 788, '2号浊环水泵', 'tpl-tag-loop-2', {
      labelX: 1114, labelW: 92, labelFontSize: 12, tagX: 1184
    }),
    ...pumpSet('tpl-loop-1', 1358, 788, '1号浊环水泵', 'tpl-tag-loop-1', {
      labelX: 1304, labelW: 92, labelFontSize: 12, tagX: 1374
    }),
    ...pumpSet('tpl-cold-2', 1482, 812, '2号冷却水泵', 'tpl-tag-cold-2', {
      labelX: 1442, labelW: 84, labelFontSize: 12, tagX: 1494, tagY: 786
    }),
    ...pumpSet('tpl-cold-1', 1586, 812, '1号冷却水泵', 'tpl-tag-cold-1', {
      labelX: 1538, labelW: 84, labelFontSize: 12, tagX: 1598, tagY: 786
    }),
    ...pumpSet('tpl-cold-3', 1690, 812, '3号冷却水泵', 'tpl-tag-cold-3', {
      labelX: 1646, labelW: 84, labelFontSize: 12, tagX: 1702, tagY: 786
    }),
    widget('tpl-v-14', 'processValveVertical', 864, 846, 42, 70, { label: '' }),
    widget('tpl-v-4', 'processValveVertical', 1048, 846, 42, 70, { label: '' }),
    widget('tpl-v-3', 'processValveVertical', 1230, 846, 42, 70, { label: '' }),
    widget('tpl-v-2', 'processValveVertical', 1417, 846, 42, 70, { label: '' }),
    widget('tpl-v-8', 'processValveVertical', 1538, 846, 42, 70, { label: '' }),
    widget('tpl-v-7', 'processValveVertical', 1630, 846, 42, 70, { label: '' }),
    widget('tpl-v-15', 'processValve', 586, 916, 42, 35, { label: '' }),
    widget('tpl-v-9', 'processValve', 1740, 916, 42, 35, { label: '' }),
    captionLabel('txt-v-15', '15号电动蝶阀', 530, 954, 142, 22),
    captionLabel('txt-v-14', '14号', 896, 876, 48, 20, { align: 'left' }),
    captionLabel('txt-v-4', '4号', 1082, 876, 44, 20, { align: 'left' }),
    captionLabel('txt-v-3', '3号', 1264, 876, 44, 20, { align: 'left' }),
    captionLabel('txt-v-2', '2号', 1450, 876, 44, 20, { align: 'left' }),
    captionLabel('txt-v-8', '8号', 1574, 876, 44, 20, { align: 'left' }),
    captionLabel('txt-v-7', '7号', 1666, 876, 44, 20, { align: 'left' }),
    captionLabel('txt-v-9', '9号', 1722, 948, 44, 20),
    widget('tpl-out-bottom-left', 'processPipeEnd', 3, 925, 56, 22, { label: '' }),
    widget('tpl-out-right-a', 'processPipeEnd', 1780, 900, 56, 22, { label: '' }),
    widget('tpl-out-right-b', 'processPipeEnd', 1780, 943, 56, 22, { label: '' }),
    metricTag('tpl-tag-bottom-mid', 700, 956, 'm3/h', { w: 74 }),
    metricTag('tpl-tag-bottom-right', 1598, 956, 'KPa', { w: 70 }),

    // 浓密机和加药区
    captionLabel('txt-mixer-pool', '浓密池', 1114, 180, 72, 22, { align: 'left' }),
    metricTag('tpl-tag-mixer-pool', 1110, 202, 'A', { w: 62 }),
    widget('tpl-mixer', 'processMixer', 1110, 224, 400, 312, { label: '', fill: 70 }),
    captionLabel('txt-filter-room', '自磨选车间', 1716, 78, 124, 22, { fontSize: 12 }),
    captionLabel('txt-feeder', '加药机', 1700, 132, 88, 22),
    widget('tpl-feeder', 'processValveVertical', 1724, 158, 46, 64, { label: '' }),
    captionLabel('txt-lift-caption', '浓密机提耙高度：', 1558, 326, 202, 22, { align: 'right' }),
    metricTag('tpl-tag-lift', 1774, 326, 'm', { w: 76 }),
    captionLabel('txt-pressure-caption', '浓密机当前压力：', 1558, 372, 202, 22, { align: 'right' }),
    metricTag('tpl-tag-pressure', 1774, 372, 'MPa', { w: 76 }),
    widget('tpl-top-right-end', 'processPipeEnd', 1806, 98, 44, 20, { label: '' })
  ]

  const green = '#176e45'
  const slurry = '#8a2a8a'
  const mainWidth = 2

  const connections = [
    // 冲洗水总管和尾矿泵入口
    line('tpl-c-in-main', 'tpl-in-left', 'tpl-v-main', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-main-to-v12', 'tpl-v-main', 'tpl-v-12', {
      fromPort: 'right', toPort: 'left', width: mainWidth, color: green,
      waypoints: [[520, 171], [520, 231]]
    }),
    line('tpl-c-v12-v11', 'tpl-v-12', 'tpl-v-11', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-main-p1', 'tpl-v-main', 'tpl-pump-1', {
      fromPort: 'bottom', toPort: 'left', width: mainWidth, color: green,
      waypoints: [[299, 249], [548, 249]]
    }),
    line('tpl-c-p1-v12', 'tpl-pump-1', 'tpl-v-12', {
      fromPort: 'right', toPort: 'left', width: mainWidth, color: green,
      waypoints: [[678, 249], [678, 231]]
    }),
    line('tpl-c-main-p2', 'tpl-v-main', 'tpl-pump-2', {
      fromPort: 'bottom', toPort: 'left', width: mainWidth, color: green,
      waypoints: [[299, 335], [596, 335]]
    }),
    line('tpl-c-p2-v21', 'tpl-pump-2', 'tpl-v-21', {
      fromPort: 'right', toPort: 'left', width: mainWidth, color: green,
      waypoints: [[680, 335], [680, 318]]
    }),
    line('tpl-c-v21-v22', 'tpl-v-21', 'tpl-v-22', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),

    // 尾矿库进线
    line('tpl-c-main-dam', 'tpl-v-main', 'tpl-dam', {
      fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[299, 379]]
    }),
    line('tpl-c-v21-dam', 'tpl-v-21', 'tpl-dam', {
      fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[763, 379]]
    }),

    // 矿浆到浓密机，使用紫色管线
    line('tpl-c-v11-mixer', 'tpl-v-11', 'tpl-mixer', {
      fromPort: 'right', toPort: 'left', width: mainWidth, color: slurry,
      waypoints: [[1056, 231], [1056, 343], [1142, 343]]
    }),
    line('tpl-c-v22-mixer', 'tpl-v-22', 'tpl-mixer', {
      fromPort: 'right', toPort: 'bottom', width: mainWidth, color: slurry,
      waypoints: [[1056, 318], [1056, 518], [1310, 518]]
    }),
    line('tpl-c-mixer-feeder', 'tpl-mixer', 'tpl-feeder', {
      fromPort: 'top', toPort: 'top', width: mainWidth, color: slurry,
      waypoints: [[1310, 110], [1747, 110]]
    }),
    line('tpl-c-feeder-top-end', 'tpl-feeder', 'tpl-top-right-end', {
      fromPort: 'top', toPort: 'right', width: mainWidth, color: slurry,
      waypoints: [[1747, 110]]
    }),
    line('tpl-c-mixer-cold', 'tpl-mixer', 'tpl-pool-cold', {
      fromPort: 'right', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[1608, 417], [1608, 551]]
    }),

    // 尾矿库到水罐，再到回水泵
    line('tpl-c-dam-t5', 'tpl-dam', 'tpl-tank-5', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-dam-t6', 'tpl-dam', 'tpl-tank-6', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-dam-t1', 'tpl-dam', 'tpl-tank-1', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-dam-t2', 'tpl-dam', 'tpl-tank-2', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-dam-t3', 'tpl-dam', 'tpl-tank-3', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-dam-t4', 'tpl-dam', 'tpl-tank-4', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-t5-r5', 'tpl-tank-5', 'tpl-return-5', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-t6-r6', 'tpl-tank-6', 'tpl-return-6', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-t1-r1', 'tpl-tank-1', 'tpl-return-1', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-t2-r2', 'tpl-tank-2', 'tpl-return-2', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-t3-r3', 'tpl-tank-3', 'tpl-return-3', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-t4-r4', 'tpl-tank-4', 'tpl-return-4', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-out-r5', 'tpl-out-left', 'tpl-return-5', {
      fromPort: 'right', toPort: 'left', width: mainWidth, color: green,
      waypoints: [[80, 726], [80, 651]]
    }),
    line('tpl-c-r5-r6', 'tpl-return-5', 'tpl-return-6', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-r6-r1', 'tpl-return-6', 'tpl-return-1', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-r1-r2', 'tpl-return-1', 'tpl-return-2', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-r2-r3', 'tpl-return-2', 'tpl-return-3', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-r3-r4', 'tpl-return-3', 'tpl-return-4', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-r4-prod', 'tpl-return-4', 'tpl-pool-prod', {
      fromPort: 'right', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[676, 651], [676, 452], [942, 452]]
    }),

    // 水池串联
    line('tpl-c-prod-cold', 'tpl-pool-prod', 'tpl-pool-cold', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),
    line('tpl-c-cold-acc', 'tpl-pool-cold', 'tpl-pool-accident', { fromPort: 'right', toPort: 'left', width: mainWidth, color: green }),

    // 水池到下方泵组
    line('tpl-c-prod-loop4', 'tpl-pool-prod', 'tpl-loop-4', {
      fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[827, 738]]
    }),
    line('tpl-c-prod-loop3', 'tpl-pool-prod', 'tpl-loop-3', {
      fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[1012, 738]]
    }),
    line('tpl-c-prod-loop2', 'tpl-pool-prod', 'tpl-loop-2', {
      fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[1199, 738]]
    }),
    line('tpl-c-cold-loop1', 'tpl-pool-cold', 'tpl-loop-1', {
      fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green,
      waypoints: [[1392, 738]]
    }),
    line('tpl-c-cold-c2', 'tpl-pool-cold', 'tpl-cold-2', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-cold-c1', 'tpl-pool-cold', 'tpl-cold-1', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-acc-c3', 'tpl-pool-accident', 'tpl-cold-3', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),

    // 泵组到阀门
    line('tpl-c-loop4-v14', 'tpl-loop-4', 'tpl-v-14', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-loop3-v4', 'tpl-loop-3', 'tpl-v-4', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-loop2-v3', 'tpl-loop-2', 'tpl-v-3', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-loop1-v2', 'tpl-loop-1', 'tpl-v-2', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-cold2-v8', 'tpl-cold-2', 'tpl-v-8', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-cold1-v7', 'tpl-cold-1', 'tpl-v-7', { fromPort: 'bottom', toPort: 'top', width: mainWidth, color: green }),
    line('tpl-c-cold3-v9', 'tpl-cold-3', 'tpl-v-9', {
      fromPort: 'bottom', toPort: 'left', width: mainWidth, color: green,
      waypoints: [[1725, 933]]
    }),

    // 底部出水总管
    line('tpl-c-bottom-left', 'tpl-out-bottom-left', 'tpl-v-15', { fromPort: 'right', toPort: 'left', width: 3, color: green }),
    line('tpl-c-v15-v14', 'tpl-v-15', 'tpl-v-14', {
      fromPort: 'right', toPort: 'bottom', width: 3, color: green,
      waypoints: [[631, 933], [631, 916]]
    }),
    line('tpl-c-v14-v4', 'tpl-v-14', 'tpl-v-4', { fromPort: 'bottom', toPort: 'bottom', width: 3, color: green, waypoints: [[977, 916]] }),
    line('tpl-c-v4-v3', 'tpl-v-4', 'tpl-v-3', { fromPort: 'bottom', toPort: 'bottom', width: 3, color: green, waypoints: [[1160, 916]] }),
    line('tpl-c-v3-v2', 'tpl-v-3', 'tpl-v-2', { fromPort: 'bottom', toPort: 'bottom', width: 3, color: green, waypoints: [[1344, 916]] }),
    line('tpl-c-v2-v8', 'tpl-v-2', 'tpl-v-8', { fromPort: 'bottom', toPort: 'bottom', width: 3, color: green, waypoints: [[1498, 916]] }),
    line('tpl-c-v8-v7', 'tpl-v-8', 'tpl-v-7', { fromPort: 'bottom', toPort: 'bottom', width: 3, color: green, waypoints: [[1605, 916]] }),
    line('tpl-c-v7-v9', 'tpl-v-7', 'tpl-v-9', {
      fromPort: 'bottom', toPort: 'left', width: 3, color: green,
      waypoints: [[1743, 916]]
    }),
    line('tpl-c-v9-out-a', 'tpl-v-9', 'tpl-out-right-a', {
      fromPort: 'right', toPort: 'left', width: 3, color: green,
      waypoints: [[1779, 911]]
    }),
    line('tpl-c-v9-out-b', 'tpl-v-9', 'tpl-out-right-b', {
      fromPort: 'right', toPort: 'left', width: 3, color: green,
      waypoints: [[1779, 954]]
    })
  ]

  return { widgets, connections }
}

export const BUILT_IN_TEMPLATES = [
  {
    id: 'tailings-water',
    name: '尾矿供水监控模板',
    description: '按参考图重新绘制的尾矿供水监控画面，去掉最底部外引标签，保留顶部状态栏、尾矿库、水罐、泵组、浓密机、水池和主要管线布局。',
    createLayout: createTailingsWaterTemplate
  }
]
