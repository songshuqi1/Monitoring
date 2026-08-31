<template>
  <div
    class="canvas-page"
    :class="{
      'resizing-library': libraryResizing,
      'resizing-bottom-dock': bottomDockResizing
    }"
  >
    <!-- 左侧：功能块库-->
    <aside class="library-pane" :style="{ width: `${libraryWidth}px` }">
      <div class="library-header">
        <h3>组件库</h3>
        <p>按行业场景和公共组件组织</p>
      </div>
      <div class="library-search">
        <input
          v-model="librarySearch"
          type="search"
          placeholder="搜索组件、分类或场景"
          @keydown.stop
          @keyup.stop
        />
        <button v-if="librarySearch" type="button" title="清空搜索" @click="librarySearch = ''">清除</button>
      </div>
      <div class="library-import">
        <input ref="scadaAssetInput" class="hidden-file-input" type="file" accept=".svg,image/svg+xml,image/png,image/jpeg,image/webp,image/gif" @change="importScadaAsset" />
        <button type="button" class="btn btn-sm" @click="scadaAssetInput?.click()">导入 SVG/图片</button>
        <span>作为可绑定组件加入画布</span>
      </div>
      <div class="library-categories">
        <div v-if="visibleCategories.length === 0" class="library-empty">
          没有找到匹配组件
        </div>
        <div v-for="cat in visibleCategories" :key="cat.key" class="lib-category">
          <div class="cat-header" @click="toggleLibraryCategory(cat)">
            <span class="cat-toggle">{{ isLibraryCategoryOpen(cat) ? '▾' : '▸' }}</span>
            <span class="cat-text">
              <span class="cat-name">{{ cat.label }}</span>
              <span v-if="cat.description" class="cat-desc">{{ cat.description }}</span>
            </span>
            <span class="cat-count">{{ categoryItemCount(cat) }}</span>
          </div>
          <div v-show="isLibraryCategoryOpen(cat)" class="cat-items">
            <template v-if="cat.groups">
              <div v-for="group in cat.groups" :key="group.key" class="lib-subcategory">
                <div class="subcat-header" @click.stop="toggleLibraryGroup(group)">
                  <span class="subcat-toggle">{{ isLibraryGroupOpen(group) ? '▾' : '▸' }}</span>
                  <span class="subcat-text">
                    <span class="subcat-name">{{ group.label }}</span>
                    <span v-if="group.description" class="subcat-desc">{{ group.description }}</span>
                  </span>
                  <span class="subcat-count">{{ group.items.length }}</span>
                </div>
                <div v-show="isLibraryGroupOpen(group)" class="subcat-items">
                  <div
                    v-for="item in group.items"
                    :key="item.scadaAsset?.id || item.customComponentId || item.type"
                    class="lib-item"
                    :style="{ borderLeftColor: item.color }"
                    draggable="true"
                    @mouseenter="showLibraryPreview(item, $event)"
                    @mousemove="moveLibraryPreview"
                    @mouseleave="hideLibraryPreview"
                    @dragstart="onDragStart($event, item)"
                    @dragend="onLibraryDragEnd"
                    @pointerup.stop="onLibraryItemPointerUp($event, item)"
                    @click.stop="onLibraryItemClick($event, item)"
                  >
                    <span class="lib-item-name" :title="item.label">{{ item.label }}</span>
                    <button v-if="item.scadaAsset" type="button" class="lib-item-delete" title="从导入组件库删除"
                      @mousedown.stop @pointerup.stop @click.stop="requestDeleteScadaAsset(item.scadaAsset)">×</button>
                  </div>
                </div>
              </div>
            </template>
            <template v-else>
              <div
                v-for="item in cat.items"
                :key="item.scadaAsset?.id || item.customComponentId || item.type"
                class="lib-item"
                :style="{ borderLeftColor: item.color }"
                draggable="true"
                @mouseenter="showLibraryPreview(item, $event)"
                @mousemove="moveLibraryPreview"
                @mouseleave="hideLibraryPreview"
                @dragstart="onDragStart($event, item)"
                @dragend="onLibraryDragEnd"
                @pointerup.stop="onLibraryItemPointerUp($event, item)"
                @click.stop="onLibraryItemClick($event, item)"
              >
                <span class="lib-item-name" :title="item.label">{{ item.label }}</span>
                <button v-if="item.scadaAsset" type="button" class="lib-item-delete" title="从导入组件库删除"
                  @mousedown.stop @pointerup.stop @click.stop="requestDeleteScadaAsset(item.scadaAsset)">×</button>
              </div>
            </template>
          </div>
        </div>
      </div>
      <div
        v-if="libraryPreview.open && libraryPreviewWidget"
        class="library-preview-popover"
        :style="{ left: `${libraryPreview.x}px`, top: `${libraryPreview.y}px` }"
      >
        <div class="library-preview-title">{{ libraryPreview.item?.label }}</div>
        <div class="library-preview-stage" :style="libraryPreviewStageStyle">
          <WidgetFactory :widget="libraryPreviewWidget" readonly />
        </div>
      </div>
    </aside>

    <div class="pane-resizer" title="拖动调整组件库宽度" @mousedown.prevent="startLibraryResize"></div>

    <!-- 中间：画布-->
    <div class="canvas-container">
      <div class="canvas-toolbar-mini">
        <button class="btn btn-sm" @click="resetLayout" title="重置布局">↺重置</button>
        <button class="btn btn-sm" @click="openProjectDialog" title="项目管理">📁 项目</button>
        <button
          class="btn btn-sm"
          :class="{ 'btn-primary': connectionMode }"
          @click="toggleConnectionMode"
          title="点击两个模块创建连接线"
        >
          {{ connectionMode ? '退出连线' : '连线' }}
        </button>
        <button class="btn btn-sm btn-danger" :disabled="!selectedConnectionId" @click="removeSelectedConnection" title="删除选中的连接线">删除线</button>
        <button class="btn btn-sm" :disabled="!selectedConnectionId" @click="resetSelectedConnectionRoute" title="清除手动拐点并重新计算最短路径">自动布线</button>
        <button class="btn btn-sm btn-primary" @click="enterApplicationMode" title="只显示画布并锁定布局">应用模式</button>
        <span v-if="connectionMode" class="connection-hint">
          拖拽端口连线；Alt 直线斜连；Shift+端口进入逐点路径；Ctrl 自由端点或分支
        </span>
        <span v-if="connectionNotice" class="connection-notice">{{ connectionNotice }}</span>
        <span class="canvas-info">{{ store.widgets.length }} 个组件 | {{ store.variables.length }} 个变量</span>
      </div>
      <div class="canvas-and-dock">
      <div
        class="canvas-area"
        :class="{ zooming: isZooming }"
        :style="canvasAreaStyle"
        ref="canvasAreaRef"
        @dragover.prevent
        @drop.prevent="onDrop"
        @mousedown.middle="startCanvasPan"
        @wheel.prevent="handleWheel"
        @mousedown.self="startAreaSelection"
        @click.self="handleCanvasBlankClick"
      >
        <div
          class="canvas-viewport"
          :style="editorViewportStyle"
          :data-zoom-refresh="zoomRefreshTick"
          @mousedown.middle="startCanvasPan"
          @mousedown.self="startAreaSelection"
          @click.self="handleCanvasBlankClick"
        >
        <div class="canvas-spacer" :style="editorCanvasSizeStyle"></div>
        <svg
          class="connection-layer"
          :viewBox="`0 0 ${editorCanvasBounds.width} ${editorCanvasBounds.height}`"
          :style="editorCanvasSizeStyle"
          preserveAspectRatio="none"
          @mousedown.middle="startCanvasPan"
          @mousedown.self="startAreaSelection"
          @click.self="handleCanvasBlankClick"
        >
          <defs>
            <marker
              v-for="rendered in renderedConnections"
              :key="`end-marker-${rendered.connection.id}`"
              :id="connectionMarkerId(rendered.connection, 'end')"
              :markerWidth="connectionArrowSize(rendered.connection)"
              :markerHeight="connectionArrowSize(rendered.connection)"
              refX="9"
              refY="5"
              orient="auto"
              markerUnits="userSpaceOnUse"
              viewBox="0 0 10 10"
            >
              <path d="M1 1L9 5L1 9Z" fill="context-stroke" />
            </marker>
            <marker
              v-for="rendered in renderedConnections"
              :key="`start-marker-${rendered.connection.id}`"
              :id="connectionMarkerId(rendered.connection, 'start')"
              :markerWidth="connectionArrowSize(rendered.connection)"
              :markerHeight="connectionArrowSize(rendered.connection)"
              refX="1"
              refY="5"
              orient="auto-start-reverse"
              markerUnits="userSpaceOnUse"
              viewBox="0 0 10 10"
            >
              <path d="M9 1L1 5L9 9Z" fill="context-stroke" />
            </marker>
          </defs>
          <g
            v-for="rendered in renderedConnections"
            :key="rendered.connection.id"
            class="connection-group"
            :data-connection-id="rendered.connection.id"
          >
            <path
              class="connection-hit"
              :d="rendered.path"
              @mousedown.stop="startConnectionFromLine(rendered.connection, $event)"
              @click.stop="selectConnection(rendered.connection.id)"
              @dblclick.stop.prevent="insertConnectionWaypointAtCursor(rendered.connection, $event)"
            />
            <path
              class="connection-outline"
              :d="rendered.path"
              :stroke-width="(Number(rendered.connection.width) || 1) + 1"
            />
            <path
              class="connection-line"
              :class="{ selected: selectedConnectionId === rendered.connection.id }"
              :d="rendered.path"
              :stroke="rendered.connection.color || '#178447'"
              :stroke-width="rendered.connection.width || 1"
              :marker-start="connectionMarkerStart(rendered.connection)"
              :marker-end="connectionMarkerEnd(rendered.connection)"
              @mousedown.stop="startConnectionFromLine(rendered.connection, $event)"
              @click.stop="selectConnection(rendered.connection.id)"
              @dblclick.stop.prevent="insertConnectionWaypointAtCursor(rendered.connection, $event)"
            />
            <circle
              v-for="handle in rendered.insertHandles"
              :key="`${rendered.connection.id}-insert-${handle.segmentIndex}`"
              class="connection-insert-handle"
              :class="[
                `segment-${handle.orientation}`,
                { 'alignment-hot': activeAlignmentHandle?.type === 'segment' && activeAlignmentHandle?.connId === rendered.connection.id && activeAlignmentHandle?.segmentIndex === handle.segmentIndex }
              ]"
              :cx="handle.x"
              :cy="handle.y"
              r="7"
              title="拖动以平行移动整段连线"
              @mousedown.stop.prevent="startConnectionInsertDrag(rendered.connection, handle.segmentIndex, $event)"
            />
            <circle
              v-if="selectedConnectionId === rendered.connection.id && rendered.handleVisible"
              class="connection-handle"
              :class="{ 'alignment-hot': activeAlignmentHandle?.type === 'bend' && activeAlignmentHandle?.connId === rendered.connection.id }"
              :cx="rendered.handle.x"
              :cy="rendered.handle.y"
              r="8"
              @mousedown.stop.prevent="startConnectionBendDrag(rendered.connection, $event)"
            />
            <circle
              v-for="handle in rendered.waypointHandles"
              :key="`${rendered.connection.id}-waypoint-${handle.index}`"
              class="connection-waypoint-handle"
              :class="{ 'alignment-hot': isWaypointAlignmentHot(rendered.connection, handle.index) }"
              :cx="handle.x"
              :cy="handle.y"
              r="8"
              @mousedown.stop.prevent="startConnectionWaypointDrag(rendered.connection, handle.index, $event)"
            />
          </g>
          <line
            v-if="orthogonalGuide"
            class="orthogonal-guide-line"
            v-bind="orthogonalGuideLineAttrs"
          />
          <path
            v-if="rubberBand"
            class="rubber-band-line"
            :class="{ 'alignment-hot': rubberBand.snapped, 'manual-trace': rubberBand.manual }"
            :d="rubberBandPath"
          />
          <circle
            v-for="(point, index) in (rubberBand?.fixedPoints || [])"
            :key="`rubber-waypoint-${index}`"
            class="rubber-band-waypoint"
            :cx="editorCanvasX(point.x)"
            :cy="editorCanvasY(point.y)"
            r="5"
          />
          <circle
            v-if="rubberBand"
            class="rubber-band-point"
            :class="{ 'alignment-hot': rubberBand.snapped }"
            :cx="editorCanvasX(rubberBand.end.x)"
            :cy="editorCanvasY(rubberBand.end.y)"
            r="8"
          />
        </svg>
        <WidgetWrapper
          v-for="w in orderedEditorWidgets"
          :key="w.id"
          :widget="w"
          :canvas-offset="editorCanvasOffset"
          :selected="selectedId === w.id || selectedWidgetSet.has(w.id)"
          :connection-mode="connectionMode"
          :connection-source="connectionDraftFrom === w.id"
          :force-show-ports="Boolean(rubberBand)"
          @select="handleWidgetSelect(w.id)"
          @port-connection-start="startPortConnection"
          @custom-port-connection-start="startCustomPortConnection"
          @open-config="openConfig(w.id)"
          @request-remove="openWidgetDelete"
        />
        <div
          v-for="w in frameBoxHitWidgets"
          :key="`frame-hit-${w.id}`"
          class="frame-box-hit-proxy"
          :style="frameBoxHitProxyStyle(w)"
          @mousedown.stop.prevent="selectFrameBoxFromProxy(w.id)"
        >
          <span class="frame-hit-edge hit-top"></span>
          <span class="frame-hit-edge hit-right"></span>
          <span class="frame-hit-edge hit-bottom"></span>
          <span class="frame-hit-edge hit-left"></span>
        </div>
        <div
          v-if="areaSelection"
          class="area-selection-box"
          :style="areaSelectionStyle"
        ></div>
        <div
          v-if="selectedWidgetIds.length"
          class="area-selection-toolbar"
          :style="areaSelectionToolbarStyle"
          @mousedown.stop
          @click.stop
        >
          <span>已选 {{ selectedWidgetIds.length }} 个</span>
          <button type="button" class="btn btn-sm" @click="copySelectedWidgets">整体复制</button>
          <button type="button" class="btn btn-sm btn-danger" @click="deleteSelectedWidgets">整体删除</button>
        </div>
      </div>
      </div>
        <BottomDock
          :height="bottomDockHeight"
          :viewport="minimapViewport"
          @resize-start="startBottomDockResize"
          @minimap-navigate="navigateCanvasOverview"
        />
      </div>
    </div>

    <!-- 右侧：配置面板-->
    <aside v-if="configWidget" :key="configWidget.id" class="config-pane">
      <div class="config-header">
        <div>
          <h3>属性 — {{ typeLabel(configWidget.type) }}</h3>
          <div class="config-instance">实例 {{ configWidget.id }}</div>
        </div>
        <button class="btn-icon" @click="closeConfig">✕</button>
      </div>
      <div class="config-body">
        <div class="form-group">
          <label>标题</label>
          <input v-model="activeConfig.title" @input="applyConfig" />
        </div>
        <label v-if="configWidget.type !== 'label'" class="inline-check name-toggle">
          <input type="checkbox" v-model="activeConfig.hideName" @change="applyConfig" />
          隐藏模块名称
        </label>
        <div v-if="configWidget.type !== 'label'" class="form-group">
          <label>标题字号</label>
          <input type="number" min="8" max="72" v-model.number="activeConfig.titleFontSize" @input="applyConfig" />
        </div>
        <div class="config-section">
          <div class="section-title">模块旋转</div>
          <div class="form-row two-col">
            <div class="form-group">
              <label>角度</label>
              <input type="number" step="1" v-model.number="activeConfig.rotation" @input="normalizeActiveRotation" />
            </div>
            <div class="form-group">
              <label>步长</label>
              <input type="number" min="1" max="180" step="1" v-model.number="activeConfig.rotationStep" @input="normalizeRotationStep" />
            </div>
          </div>
          <div class="rotation-action-row">
            <button type="button" class="btn btn-sm" @click="rotateActiveWidget(-1)">左旋</button>
            <button type="button" class="btn btn-sm" @click="resetActiveRotation">归零</button>
            <button type="button" class="btn btn-sm" @click="rotateActiveWidget(1)">右旋</button>
          </div>
        </div>
        <div v-if="isProcessConfig" class="form-group">
          <label>显示名称</label>
          <input v-model="activeConfig.label" @input="applyConfig" placeholder="例如：1号回水泵" />
        </div>
        <div v-if="isProcessConfig && configWidget.type !== 'processValueTag'" class="form-group">
          <label>名称字号</label>
          <input type="number" min="8" max="72" v-model.number="activeConfig.labelFontSize" @input="applyConfig" />
        </div>
        <div v-if="['processPool', 'processTank', 'processMixer', 'processAluminaSilo'].includes(configWidget.type)" class="form-group">
          <label>液位显示（%）</label>
          <input type="number" min="0" max="100" v-model.number="activeConfig.fill" @input="applyConfig" />
        </div>
        <label v-if="configWidget.type === 'processJunction'" class="inline-check name-toggle">
          <input type="checkbox" v-model="activeConfig.hideNode" @change="applyConfig" />
          隐藏连接节点
        </label>

        <!-- 变量绑定 -->
        <div v-if="configWidget.type === 'processPump'" class="label-style-toggles">
          <label><input type="checkbox" v-model="activeConfig.flipX" @change="applyConfig" /> 水平翻转</label>
          <label><input type="checkbox" v-model="activeConfig.flipY" @change="applyConfig" /> 垂直翻转</label>
        </div>
        <div v-if="showVarSelect" class="form-group">
          <label>{{ isTrend ? '显示变量（勾选要显示的变量）' : '关联变量' }}</label>
          <div class="var-binding-search">
            <input v-model.trim="variableBindingSearch" type="search" placeholder="搜索名称、描述、来源或单位" />
            <span>{{ visibleBindingVariableCount }}/{{ store.variables.length }}</span>
          </div>

          <!-- 趋势图：多选 checkbox -->
          <div v-if="isTrend" class="var-checkbox-list">
            <div v-for="group in configVariableGroups" :key="group.key" class="var-select-group">
              <button type="button" class="var-select-group-title" @click="toggleConfigVariableGroup(group.key)">
                <span class="var-select-group-name"><b>{{ isConfigVariableGroupCollapsed(group.key) ? '▸' : '▾' }}</b>{{ group.label }}</span>
                <span>{{ group.items.length }}</span>
              </button>
              <div v-show="!isConfigVariableGroupCollapsed(group.key)" class="var-select-group-items">
                <label v-for="v in group.items" :key="v.id" class="var-checkbox">
                  <input type="checkbox" :value="v.id"
                    :checked="(activeConfig.varIds || []).includes(v.id)"
                    @change="toggleTrendVar(v.id)" />
                  <span class="var-checkbox-name" :style="{ borderLeftColor: v.color || '#4f6fb8' }">{{ v.name }}</span>
                  <span class="var-checkbox-meta">{{ v.unit || '' }} {{ v.source ? '('+v.source+')' : '' }}</span>
                </label>
              </div>
            </div>
            <div v-if="store.variables.length === 0" class="text-muted" style="font-size:13px;padding:8px 0;">
              暂无变量定义，请先在系统设置中添加变量
            </div>
          </div>

            <!-- 趋势图坐标范围-->
            <template v-if="isTrend">
              <div class="form-row" style="display:flex;gap:8px;margin-top:8px;">
                <div class="form-group" style="flex:1;margin-bottom:0;">
                  <label>Y 轴最小值</label>
                  <input type="number" step="any" v-model.number="activeConfig.yMin" @input="applyConfig" placeholder="自动" />
                </div>
                <div class="form-group" style="flex:1;margin-bottom:0;">
                  <label>Y 轴最大值</label>
                  <input type="number" step="any" v-model.number="activeConfig.yMax" @input="applyConfig" placeholder="自动" />
                </div>
              </div>
              <div class="form-group" style="margin-top:8px;">
                <label>时间范围（秒）</label>
                <input type="number" min="10" max="600" v-model.number="activeConfig.timeWindow" @input="applyConfig" placeholder="默认60秒" />
              </div>
            </template>
          <!-- 其他 widget：单选 select -->
          <select v-else v-model.number="activeConfig.varId" @change="applyConfig">
            <option :value="null">-- 选择变量 --</option>
            <optgroup v-for="group in configVariableGroups" :key="group.key" :label="`${group.label} (${group.items.length})`">
              <option v-for="v in group.items" :key="v.id" :value="v.id">
                {{ v.name }} | {{ v.description || v.source || '-' }} {{ v.unit ? ' ['+v.unit+']' : '' }}
              </option>
            </optgroup>
          </select>

          <!-- 已选变量信息-->
          <div v-if="selectedVarInfo" class="var-info-card">
            <div class="var-info-row"><span>OPC UA 节点</span><span style="font-family:var(--font-mono);font-size:12px;">\Root\Objects\{{ selectedVarInfo.name }}</span></div>
            <div class="var-info-row"><span>NodeId</span><span style="font-family:var(--font-mono);font-size:12px;">ns=1;s={{ selectedVarInfo.name }}</span></div>
            <div class="var-info-row"><span>来源</span><span>{{ selectedVarInfo.source || '-' }}</span></div>
            <div class="var-info-row"><span>范围</span><span>{{ selectedVarInfo.minValue ?? '-' }} ~ {{ selectedVarInfo.maxValue ?? '-' }} {{ selectedVarInfo.unit || '' }}</span></div>
            <div class="var-info-row"><span>质量</span><span class="quality-dot" :class="getVarQuality(selectedVarInfo.id)"></span></div>
          </div>
        </div>

        <div v-if="isCustomShapeConfig" class="config-section">
          <div class="section-title">状态颜色</div>
          <label class="inline-check"><input type="checkbox" v-model="activeConfig.bindVariable" @change="syncCustomShapeRuntimeConfig" /> 绑定变量控制颜色</label>
          <div class="form-row two-col">
            <div class="form-group"><label>开颜色</label><input type="color" v-model="activeConfig.statusOnColor" @input="syncCustomShapeRuntimeConfig" /></div>
            <div class="form-group"><label>关颜色</label><input type="color" v-model="activeConfig.statusOffColor" @input="syncCustomShapeRuntimeConfig" /></div>
          </div>
        </div>

        <div v-if="isScadaSvgConfig" class="config-section">
          <div class="section-title">导入组件样式</div>
          <label class="inline-check"><input type="checkbox" v-model="activeConfig.bindVariable" @change="applyConfig" /> 绑定变量控制状态颜色</label>
          <div class="form-row two-col">
            <div class="form-group"><label>开启颜色</label><input type="color" v-model="activeConfig.statusOnColor" @input="applyConfig" /></div>
            <div class="form-group"><label>关闭颜色</label><input type="color" v-model="activeConfig.statusOffColor" @input="applyConfig" /></div>
          </div>
          <div class="form-row two-col">
            <div class="form-group"><label>透明度 (%)</label><input type="number" min="0" max="100" v-model.number="activeConfig.opacity" @input="applyConfig" /></div>
            <div class="form-group"><label>缩放方式</label><select v-model="activeConfig.preserveAspectRatio" @change="applyConfig"><option value="meet">完整显示</option><option value="slice">铺满裁切</option><option value="none">拉伸填满</option></select></div>
          </div>
        </div>

        <div v-if="isConnectableConfig" class="config-section">
          <div class="section-title">连接端口</div>
          <div class="port-config-list">
            <div v-for="side in PORT_CONFIG_SIDES" :key="side" class="port-config-row">
              <label>
                <input
                  type="checkbox"
                  :checked="isConfigPortEnabled(side)"
                  @change="toggleConfigPort(side, $event.target.checked)"
                />
                {{ portSideLabel(side) }}
              </label>
              <input
                type="range"
                min="0"
                max="100"
                :disabled="!isConfigPortEnabled(side)"
                :value="configPortOffset(side)"
                @input="setConfigPortOffset(side, $event.target.value)"
              />
              <span>{{ configPortOffset(side) }}%</span>
            </div>
          </div>
        </div>

        <!-- 数值范围（仪表盘数值显示） -->
        <template v-if="['gauge', 'digitalDisplay', 'valueColumn', 'stepperControl'].includes(configWidget.type)">
          <div class="form-group"><label>最小值</label><input type="number" v-model.number="activeConfig.min" @input="applyConfig" /></div>
          <div class="form-group"><label>最大值</label><input type="number" v-model.number="activeConfig.max" @input="applyConfig" /></div>
        </template>
        <div v-if="['gauge', 'valueColumn', 'stepperControl'].includes(configWidget.type)" class="form-group"><label>单位</label><input v-model="activeConfig.unit" @input="applyConfig" /></div>
        <template v-if="configWidget.type === 'label'">
          <div class="form-group"><label>文本</label><input v-model="activeConfig.text" @input="applyConfig" /></div>
          <div class="form-row two-col">
            <div class="form-group"><label>字号</label><input type="number" min="8" max="72" v-model.number="activeConfig.fontSize" @input="applyConfig" /></div>
            <div class="form-group"><label>颜色</label><input type="color" v-model="activeConfig.color" @input="applyConfig" /></div>
          </div>
          <div class="label-style-toggles">
            <label><input type="checkbox" v-model="activeConfig.bold" @change="applyConfig" /> 加粗</label>
            <label><input type="checkbox" v-model="activeConfig.italic" @change="applyConfig" /> 斜体</label>
            <label><input type="checkbox" v-model="activeConfig.underline" @change="applyConfig" /> 下划线</label>
          </div>
          <div class="form-row two-col">
            <div class="form-group">
              <label>水平对齐</label>
              <select v-model="activeConfig.align" @change="applyConfig">
                <option value="left">左对齐</option>
                <option value="center">居中</option>
                <option value="right">右对齐</option>
              </select>
            </div>
            <div class="form-group">
              <label>垂直对齐</label>
              <select v-model="activeConfig.verticalAlign" @change="applyConfig">
                <option value="top">顶部</option>
                <option value="center">居中</option>
                <option value="bottom">底部</option>
              </select>
            </div>
          </div>
          <div class="form-row two-col">
            <div class="form-group"><label>背景色</label><input type="color" v-model="activeConfig.background" @input="applyConfig" /></div>
            <div class="form-group"><label>边框色</label><input type="color" v-model="activeConfig.borderColor" @input="applyConfig" /></div>
          </div>
          <div class="form-row two-col">
            <div class="form-group"><label>边框宽度</label><input type="number" min="0" max="12" v-model.number="activeConfig.borderWidth" @input="applyConfig" /></div>
            <div class="form-group"><label>圆角</label><input type="number" min="0" max="40" v-model.number="activeConfig.borderRadius" @input="applyConfig" /></div>
          </div>
        </template>
        <template v-if="configWidget.type === 'frameBox'">
          <div class="form-row two-col">
            <div class="form-group"><label>背景色</label><input type="color" v-model="activeConfig.background" @input="applyConfig" /></div>
            <div class="form-group"><label>背景透明度（%）</label><input type="number" min="0" max="100" v-model.number="activeConfig.backgroundOpacity" @input="applyConfig" /></div>
          </div>
          <div class="form-row two-col">
            <div class="form-group"><label>边框色</label><input type="color" v-model="activeConfig.borderColor" @input="applyConfig" /></div>
            <div class="form-group"><label>边框宽度</label><input type="number" min="0" max="40" v-model.number="activeConfig.borderWidth" @input="applyConfig" /></div>
          </div>
          <div class="form-row two-col">
            <div class="form-group"><label>圆角</label><input type="number" min="0" max="120" v-model.number="activeConfig.borderRadius" @input="applyConfig" /></div>
            <div class="form-group">
              <label>线型</label>
              <select v-model="activeConfig.borderStyle" @change="applyConfig">
                <option value="solid">实线</option>
                <option value="dashed">虚线</option>
                <option value="dotted">点线</option>
              </select>
            </div>
          </div>
        </template>
        <div v-if="configWidget.type === 'indicator'" class="form-group"><label>报警阈值</label><input type="number" v-model.number="activeConfig.threshold" @input="applyConfig" /></div>
        <div v-if="configWidget.type === 'statusCircle'" class="form-group"><label>绿色阈值</label><input type="number" v-model.number="activeConfig.threshold" @input="applyConfig" /></div>

        <!-- 小数位数 -->
        <div v-if="configWidget.type === 'button' || configWidget.type === 'winccToggleButton'" class="form-group"><label>按钮文本</label><input v-model="activeConfig.buttonText" @input="applyConfig" placeholder="按钮上显示的文字" /></div>
        <div v-if="configWidget.type === 'button'" class="form-group"><label>写入值</label><input type="number" v-model.number="activeConfig.writeValue" @input="applyConfig" /></div>
        <template v-if="configWidget.type === 'winccToggleButton'">
          <div class="form-group"><label>按下写入值</label><input type="number" v-model.number="activeConfig.writeValue" @input="applyConfig" /></div>
          <div class="form-group"><label>激活颜色</label><input type="color" v-model="activeConfig.activeColor" @input="applyConfig" /></div>
          <div class="config-section mutual-exclusion-config">
            <div class="section-title">互斥状态组</div>
            <label class="inline-check">
              <input type="checkbox" :checked="Boolean(activeConfig.exclusiveEnabled)" @change="setToggleExclusiveEnabled($event.target.checked)" />
              与选定的状态切换按钮互斥
            </label>
            <template v-if="activeConfig.exclusiveEnabled">
              <p class="mutual-exclusion-help">按“标题”勾选要互斥的状态切换按钮；按钮文本不会作为选择依据。点击任一按钮后，该按钮高亮，组内其他按钮恢复未激活颜色。</p>
              <div v-if="toggleExclusivePeerOptions.length" class="mutual-exclusion-list">
                <label v-for="peer in toggleExclusivePeerOptions" :key="peer.id" class="mutual-exclusion-item" :class="{ selected: peer.selected, disabled: Boolean(peer.titleIssue) && !peer.selected }">
                  <input type="checkbox" :checked="peer.selected" :disabled="Boolean(peer.titleIssue) && !peer.selected" @change="setToggleExclusivePeer(peer.id, $event.target.checked)" />
                  <span class="mutual-exclusion-item-text">
                    <strong>标题：{{ peer.title }}</strong>
                    <small v-if="peer.titleIssue" class="mutual-exclusion-warning">{{ peer.titleIssue }}</small>
                  </span>
                </label>
              </div>
              <p v-else class="mutual-exclusion-help">请先在画布中添加至少一个其他“状态切换按钮”。</p>
            </template>
          </div>
        </template>
        <template v-if="configWidget.type === 'stepperControl'">
          <div class="form-row two-col">
            <div class="form-group"><label>步长</label><input type="number" min="0.0001" step="any" v-model.number="activeConfig.step" @input="applyConfig" /></div>
            <div class="form-group"><label>默认值</label><input type="number" step="any" v-model.number="activeConfig.defaultValue" @input="applyConfig" /></div>
          </div>
        </template>
        <template v-if="configWidget.type === 'valueColumn'">
          <div class="form-row two-col">
            <div class="form-group"><label>液柱颜色</label><input type="color" v-model="activeConfig.barColor" @input="applyConfig" /></div>
            <div class="form-group"><label>刻度颜色</label><input type="color" v-model="activeConfig.scaleColor" @input="applyConfig" /></div>
          </div>
          <div class="form-group"><label>数值位置</label><select v-model="activeConfig.valuePosition" @change="applyConfig"><option value="right">右侧</option><option value="left">左侧</option></select></div>
        </template>
        <div v-if="configWidget.type === 'processValueTag'" class="form-group"><label>单位</label><input v-model="activeConfig.unit" @input="applyConfig" /></div>
        <div v-if="configWidget.type === 'processValueTag'" class="form-group"><label>数据字号</label><input type="number" min="8" max="72" v-model.number="activeConfig.valueFontSize" @input="applyConfig" /></div>
        <div v-if="['digitalDisplay', 'gauge', 'valueColumn', 'processValueTag', 'stepperControl'].includes(configWidget.type)" class="form-group">
          <label>小数位数</label>
          <input type="number" v-model.number="activeConfig.decimals" min="0" max="6" @input="applyConfig" />
        </div>
      </div>
    </aside>
    <aside v-else-if="selectedConnection" :key="selectedConnection.id" class="config-pane">
      <div class="config-header">
        <div>
          <h3>属性 — 连接线</h3>
          <div class="config-instance">实例 {{ selectedConnection.id }}</div>
        </div>
        <button class="btn-icon" @click="selectedConnectionId = null">✕</button>
      </div>
      <div class="config-body">
        <div class="form-row two-col">
          <div class="form-group">
            <label>线颜色</label>
            <input type="color" :value="selectedConnection.color || '#178447'" @input="updateSelectedConnection({ color: $event.target.value })" />
          </div>
          <div class="form-group">
            <label>线宽</label>
            <input type="number" min="1" max="12" :value="selectedConnection.width || 1" @input="updateSelectedConnection({ width: Number($event.target.value) || 1 })" />
          </div>
        </div>
        <div class="label-style-toggles two-items">
          <label><input type="checkbox" :checked="Boolean(selectedConnection.startArrow)" @change="updateSelectedConnection({ startArrow: $event.target.checked })" /> 首端箭头</label>
          <label><input type="checkbox" :checked="Boolean(selectedConnection.endArrow)" @change="updateSelectedConnection({ endArrow: $event.target.checked })" /> 尾端箭头</label>
        </div>
        <div class="form-group">
          <label>箭头大小</label>
          <input
            type="number"
            min="4"
            max="48"
            :value="selectedConnection.arrowSize || 10"
            @input="updateSelectedConnection({ arrowSize: Number($event.target.value) || 10 })"
          />
        </div>
        <div class="form-group">
          <label>布线方式</label>
          <select :value="selectedConnection.routeMode || ''" @change="updateSelectedConnectionRouteMode($event.target.value)">
            <option value="">自动</option>
            <option value="straight">直线（可斜）</option>
            <option value="straight-h">水平直线</option>
            <option value="straight-v">垂直直线</option>
            <option value="hv">先横后竖</option>
            <option value="vh">先竖后横</option>
          </select>
        </div>
        <button class="btn btn-sm" @click="resetSelectedConnectionRoute">自动布线</button>
      </div>
    </aside>
  </div>
    <ProjectManagerDialog
      v-if="showProjectDialog"
      :current-layout="store.layoutSnapshot"
      :open-create-on-mount="projectDialogCreateOnOpen"
      @close="closeProjectDialog"
      @load-layout="applyProjectLayout"
    />

    <ConfirmDialog
      v-if="resetConfirmOpen"
      title="重置画布"
      message="确定重置所有组件布局？"
      detail="当前画布上的组件会被清空或恢复为默认布局，此操作不会删除变量和数据库历史。"
      confirm-text="确认重置"
      danger
      @close="resetConfirmOpen = false"
      @confirm="confirmResetLayout"
    />

    <ConfirmDialog
      v-if="deleteWidgetTarget"
      title="删除组件"
      :message="`删除「${deleteWidgetTitle}」组件？`"
      detail="只会从当前画布移除该组件，不会删除变量或历史数据。"
      confirm-text="确认删除"
      danger
      @close="deleteWidgetTarget = null"
      @confirm="confirmWidgetDelete"
    />

    <ConfirmDialog
      v-if="deleteScadaAssetTarget"
      title="删除导入组件"
      :message="`确定从导入组件库删除「${deleteScadaAssetTarget.asset.name}」吗？`"
      :detail="deleteScadaAssetTarget.usedOnCanvas
        ? `画布上已有的 ${deleteScadaAssetTarget.usedOnCanvas} 个实例会保留，可继续单独编辑或删除。`
        : '仅删除组件库条目；之后仍可重新导入该组件。'"
      confirm-text="确认删除"
      danger
      @close="deleteScadaAssetTarget = null"
      @confirm="confirmDeleteScadaAsset"
    />

    <!-- 应用模式：只读画布内容，按钮等模块仍可交互 -->
    <div v-if="applicationMode" class="application-overlay">
      <div class="application-toolbar">
        <span class="application-title">应用模式</span>
        <div class="application-zoom-controls">
          <button class="application-zoom-button" title="缩小 1%" @click="zoomApplicationBy(-1)">−</button>
          <label class="application-zoom-value" title="输入 25% 至 400%，回车或失焦后生效">
            <input v-model="applicationZoomInput" type="number" min="25" max="400" step="1"
              aria-label="应用模式缩放百分比" @change="applyApplicationZoomInput" @keydown.enter.prevent="applyApplicationZoomInput" />
            <span>%</span>
          </label>
          <button class="application-zoom-button" title="放大 1%" @click="zoomApplicationBy(1)">+</button>
          <button class="application-zoom-reset" title="恢复 100%" @click="resetApplicationZoom">100%</button>
        </div>
        <button class="btn btn-sm btn-secondary" @click="exitApplicationMode">返回编辑</button>
      </div>
      <div ref="applicationCanvasRef" class="application-canvas" @wheel="handleApplicationWheel">
        <div v-if="store.widgets.length === 0" class="application-empty">暂无组件</div>
        <div v-else class="application-stage" :style="applicationStageStyle">
          <div class="application-content" :style="applicationContentStyle">
            <svg
              class="connection-layer app-connection-layer"
              :width="applicationBounds.width"
              :height="applicationBounds.height"
              :viewBox="`0 0 ${applicationBounds.width} ${applicationBounds.height}`"
              :style="applicationCanvasStyle"
              preserveAspectRatio="none"
            >
              <defs>
                <marker
                  v-for="rendered in applicationRenderedConnections"
                  :key="`app-end-marker-${rendered.connection.id}`"
                  :id="connectionMarkerId(rendered.connection, 'end', true)"
                  :markerWidth="connectionArrowSize(rendered.connection)"
                  :markerHeight="connectionArrowSize(rendered.connection)"
                  refX="9"
                  refY="5"
                  orient="auto"
                  markerUnits="userSpaceOnUse"
                  viewBox="0 0 10 10"
                >
                  <path d="M1 1L9 5L1 9Z" fill="context-stroke" />
                </marker>
                <marker
                  v-for="rendered in applicationRenderedConnections"
                  :key="`app-start-marker-${rendered.connection.id}`"
                  :id="connectionMarkerId(rendered.connection, 'start', true)"
                  :markerWidth="connectionArrowSize(rendered.connection)"
                  :markerHeight="connectionArrowSize(rendered.connection)"
                  refX="1"
                  refY="5"
                  orient="auto-start-reverse"
                  markerUnits="userSpaceOnUse"
                  viewBox="0 0 10 10"
                >
                  <path d="M9 1L1 5L9 9Z" fill="context-stroke" />
                </marker>
              </defs>
              <g v-for="rendered in applicationRenderedConnections" :key="'app-line-' + rendered.connection.id">
                <path
                  class="connection-outline"
                  :d="rendered.path"
                  :stroke-width="(Number(rendered.connection.width) || 1) + 1"
                />
                <path
                  class="connection-line"
                  :d="rendered.path"
                  :stroke="rendered.connection.color || '#178447'"
                  :stroke-width="rendered.connection.width || 1"
                  :marker-start="connectionMarkerStart(rendered.connection, true)"
                  :marker-end="connectionMarkerEnd(rendered.connection, true)"
                />
              </g>
            </svg>
            <WidgetWrapper
              v-for="w in orderedApplicationWidgets"
              :key="'app-' + w.id"
              :widget="w"
              :selected="false"
              :readonly="true"
            />
          </div>
        </div>
      </div>
    </div>

    <Teleport to="body">
      <div
        v-if="contextMenu.open"
        ref="contextMenuRef"
        class="platform-context-menu"
        :style="{ left: `${contextMenu.x}px`, top: `${contextMenu.y}px` }"
        role="menu"
        @mousedown.stop
        @contextmenu.prevent
      >
        <div class="context-menu-title">{{ contextTargetTitle }}</div>

        <template v-if="contextMenu.targetType === 'application'">
          <button class="context-menu-item" @click="runContextAction('exit-application')">
            <span>返回编辑模式</span><kbd>Esc</kbd>
          </button>
        </template>

        <template v-else-if="contextMenu.targetType === 'widget'">
          <button class="context-menu-item" @click="runContextAction('edit-widget')"><span>编辑属性</span></button>
          <button class="context-menu-item" @click="runContextAction('copy-widget')"><span>复制组件</span><kbd>Ctrl+C</kbd></button>
          <button class="context-menu-item" @click="runContextAction('duplicate-widget')"><span>创建副本</span><kbd>Ctrl+D</kbd></button>
          <div class="context-menu-separator"></div>
          <button class="context-menu-item" :disabled="!store.canUndo" @click="runContextAction('undo')"><span>撤销</span><kbd>Ctrl+Z</kbd></button>
          <button class="context-menu-item" :disabled="!store.canRedo" @click="runContextAction('redo')"><span>恢复</span><kbd>Ctrl+Y</kbd></button>
          <div class="context-menu-separator"></div>
          <button class="context-menu-item danger" @click="runContextAction('delete-widget')"><span>删除组件</span><kbd>Delete</kbd></button>
        </template>

        <template v-else-if="contextMenu.targetType === 'connection'">
          <button class="context-menu-item" @click="runContextAction('auto-route')"><span>自动布线</span></button>
          <button class="context-menu-item" :disabled="!store.canUndo" @click="runContextAction('undo')"><span>撤销</span><kbd>Ctrl+Z</kbd></button>
          <button class="context-menu-item" :disabled="!store.canRedo" @click="runContextAction('redo')"><span>恢复</span><kbd>Ctrl+Y</kbd></button>
          <div class="context-menu-separator"></div>
          <button class="context-menu-item danger" @click="runContextAction('delete-connection')"><span>删除连线</span><kbd>Delete</kbd></button>
        </template>

        <template v-else>
          <button class="context-menu-item" :disabled="!copiedWidget && !copiedSelection?.widgets?.length" @click="runContextAction('paste-widget')"><span>粘贴组件</span><kbd>Ctrl+V</kbd></button>
          <button class="context-menu-item" :disabled="!store.canUndo" @click="runContextAction('undo')"><span>撤销</span><kbd>Ctrl+Z</kbd></button>
          <button class="context-menu-item" :disabled="!store.canRedo" @click="runContextAction('redo')"><span>恢复</span><kbd>Ctrl+Y</kbd></button>
          <div class="context-menu-separator"></div>
          <button class="context-menu-item" @click="runContextAction('focus-content')"><span>定位全部内容</span><kbd>Space</kbd></button>
          <button class="context-menu-item" @click="runContextAction('clear-selection')"><span>清除选择</span></button>
          <button class="context-menu-item" @click="runContextAction('enter-application')"><span>进入应用模式</span></button>
        </template>
      </div>
    </Teleport>

</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, reactive, provide, watch } from 'vue'
import { useMonitorStore, WIDGET_TYPES } from '../store/index.js'
import { useProjectStore } from '../store/projectStore.js'
import WidgetWrapper from '../components/canvas/WidgetWrapper.vue'
import WidgetFactory from '../components/canvas/WidgetFactory.vue'
import BottomDock from '../components/BottomDock.vue'
import ProjectManagerDialog from '../components/projects/ProjectManagerDialog.vue'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'
import { vectorAnchor, vectorBounds, vectorCenter, vectorObstacleBounds, vectorPort } from '../services/vectorModelService.js'

const store = useMonitorStore()
const projectStore = useProjectStore()
const selectedId = ref(null)
const configWidgetId = ref(null)
const configDrafts = reactive({})
const canvasAreaRef = ref(null)
const canvasAreaSize = ref({ width: 0, height: 0 })
const applicationCanvasRef = ref(null)
const applicationMode = ref(false)
const applicationZoom = ref(1)
const applicationZoomInput = ref(100)
const applicationCanvasSize = ref({ width: 0, height: 0 })
const resetConfirmOpen = ref(false)
const deleteWidgetTarget = ref(null)
const deleteScadaAssetTarget = ref(null)
const connectionMode = ref(false)
const connectionDraftFrom = ref(null)
const selectedConnectionId = ref(null)
const connectionNotice = ref('')
const rubberBand = ref(null)
const orthogonalGuide = ref(null)
const activeAlignmentHandle = ref(null)
const copiedWidget = ref(null)
const copiedSelection = ref(null)
const selectedWidgetIds = ref([])
const areaSelection = ref(null)
const contextMenuRef = ref(null)
const contextMenu = reactive({
  open: false,
  x: 0,
  y: 0,
  targetType: 'canvas',
  targetId: null,
  canvasPoint: null
})
let pasteSequence = 0
let connectionNoticeTimer = null
let suppressNextCanvasClick = false
const libraryPreview = reactive({
  open: false,
  item: null,
  x: 0,
  y: 0
})
const librarySearch = ref('')
const libraryDragging = ref(false)
let lastLibraryActivationAt = 0
const scadaAssetInput = ref(null)
const SCADA_ASSET_LIBRARY_KEY = 'monitoring.scadaAssetLibrary.v1'
const scadaAssetLibrary = ref(readScadaAssetLibrary())
const variableBindingSearch = ref('')
const collapsedConfigVariableGroups = reactive({})
const LIBRARY_WIDTH_MIN = 232
const LIBRARY_WIDTH_MAX = 460
const BOTTOM_DOCK_HEIGHT_MIN = 96
const BOTTOM_DOCK_HEIGHT_MAX = 430
const libraryWidth = ref(readStoredNumber('monitoring.libraryWidth.v2', 272, LIBRARY_WIDTH_MIN, LIBRARY_WIDTH_MAX))
const bottomDockHeight = ref(readStoredNumber('monitoring.bottomDockHeight', 180, BOTTOM_DOCK_HEIGHT_MIN, BOTTOM_DOCK_HEIGHT_MAX))
const libraryResizing = ref(false)
const bottomDockResizing = ref(false)
let libraryResizeState = null
let bottomDockResizeState = null
let libraryResizeFrame = 0
let bottomDockResizeFrame = 0
const STRAIGHT_LINE_SNAP = 24
const ORTHOGONAL_SNAP_DEGREES = 10
const PORT_ALIGNMENT_SNAP = 14
const WIDGET_ALIGNMENT_SNAP = 24
const PORT_NAMES = ['top', 'right', 'bottom', 'left']
const PORT_EXIT_LENGTH = 20
const PORT_JOIN_OVERLAP = 0
const PORT_EXIT_DIRECTIONS = {
  right: { x: 1, y: 0 },
  left: { x: -1, y: 0 },
  top: { x: 0, y: -1 },
  bottom: { x: 0, y: 1 }
}

function readStoredNumber(key, fallback, min, max) {
  if (typeof window === 'undefined') return fallback
  const value = Number(window.localStorage.getItem(key))
  return clampNumber(Number.isFinite(value) ? value : fallback, min, max)
}

function clampNumber(value, min, max) {
  return Math.max(min, Math.min(max, value))
}

function startLibraryResize(e) {
  libraryResizeState = {
    startX: e.clientX,
    startWidth: libraryWidth.value
  }
  libraryResizing.value = true
  document.addEventListener('mousemove', handleLibraryResize)
  document.addEventListener('mouseup', stopLibraryResize)
}

function handleLibraryResize(e) {
  if (!libraryResizeState) return
  const nextWidth = clampNumber(
    libraryResizeState.startWidth + e.clientX - libraryResizeState.startX,
    LIBRARY_WIDTH_MIN,
    LIBRARY_WIDTH_MAX
  )
  if (libraryResizeFrame) cancelAnimationFrame(libraryResizeFrame)
  libraryResizeFrame = requestAnimationFrame(() => {
    libraryWidth.value = Math.round(nextWidth)
    updateCanvasAreaSize()
    libraryResizeFrame = 0
  })
}

function stopLibraryResize() {
  if (libraryResizeFrame) cancelAnimationFrame(libraryResizeFrame)
  libraryResizeFrame = 0
  libraryResizeState = null
  libraryResizing.value = false
  if (typeof window !== 'undefined') {
    window.localStorage.setItem('monitoring.libraryWidth.v2', String(libraryWidth.value))
  }
  document.removeEventListener('mousemove', handleLibraryResize)
  document.removeEventListener('mouseup', stopLibraryResize)
}

function startBottomDockResize(e) {
  bottomDockResizeState = {
    startY: e.clientY,
    startHeight: bottomDockHeight.value
  }
  bottomDockResizing.value = true
  document.addEventListener('mousemove', handleBottomDockResize)
  document.addEventListener('mouseup', stopBottomDockResize)
}

function handleBottomDockResize(e) {
  if (!bottomDockResizeState) return
  const nextHeight = clampNumber(
    bottomDockResizeState.startHeight - (e.clientY - bottomDockResizeState.startY),
    BOTTOM_DOCK_HEIGHT_MIN,
    BOTTOM_DOCK_HEIGHT_MAX
  )
  if (bottomDockResizeFrame) cancelAnimationFrame(bottomDockResizeFrame)
  bottomDockResizeFrame = requestAnimationFrame(() => {
    bottomDockHeight.value = Math.round(nextHeight)
    updateCanvasAreaSize()
    bottomDockResizeFrame = 0
  })
}

function stopBottomDockResize() {
  if (bottomDockResizeFrame) cancelAnimationFrame(bottomDockResizeFrame)
  bottomDockResizeFrame = 0
  bottomDockResizeState = null
  bottomDockResizing.value = false
  if (typeof window !== 'undefined') {
    window.localStorage.setItem('monitoring.bottomDockHeight', String(bottomDockHeight.value))
  }
  document.removeEventListener('mousemove', handleBottomDockResize)
  document.removeEventListener('mouseup', stopBottomDockResize)
}

const configWidget = computed(() => {
  if (!configWidgetId.value) return null
  return store.widgets.find(w => w.id === configWidgetId.value) || null
})

const activeConfig = computed(() => {
  if (!configWidget.value) return {}
  return ensureConfigDraft(configWidget.value)
})

const toggleExclusivePeerOptions = computed(() => {
  if (!configWidget.value || configWidget.value.type !== 'winccToggleButton') return []
  const ownId = String(configWidget.value.id)
  const selectedIds = new Set(normalizeExclusivePeerIds(activeConfig.value.exclusivePeerIds))
  const buttons = store.widgets.filter(widget => widget?.type === 'winccToggleButton')
  const titles = buttons.map(widget => String(widget.config?.title || '').trim())
  const titleCounts = titles.reduce((counts, title) => {
    if (title) counts.set(title, (counts.get(title) || 0) + 1)
    return counts
  }, new Map())
  return buttons
    .filter(widget => String(widget.id) !== ownId)
    .map(widget => {
      const title = String(widget.config?.title || '').trim()
      const titleIssue = !title
        ? '未填写标题，请先填写唯一标题后再选择'
        : titleCounts.get(title) > 1
          ? '标题重复，请先修改为唯一标题后再选择'
          : ''
      return {
        id: String(widget.id),
        title: title || '未命名状态切换按钮',
        titleIssue,
        selected: selectedIds.has(String(widget.id))
      }
    })
})

const libraryPreviewWidget = computed(() => {
  const item = libraryPreview.item
  if (!item || !WIDGET_TYPES[item.type]) return null
  const def = WIDGET_TYPES[item.type]
  const customDefinition = item.customComponent || null
  const scadaAsset = item.scadaAsset || null
  return {
    id: `preview-${item.type}`,
    type: item.type,
    x: 0,
    y: 0,
    w: scadaAsset?.width || customDefinition?.width || def.defaultW,
    h: scadaAsset?.height || customDefinition?.height || def.defaultH,
    config: previewConfigForType(item.type, item.label, customDefinition, scadaAsset)
  }
})

const libraryPreviewStageStyle = computed(() => {
  const widget = libraryPreviewWidget.value
  if (!widget) return {}
  const maxW = 210
  const maxH = 150
  const scale = Math.min(maxW / widget.w, maxH / widget.h, 1.45)
  return {
    width: `${Math.max(80, Math.round(widget.w * scale))}px`,
    height: `${Math.max(48, Math.round(widget.h * scale))}px`
  }
})

const isTrend = computed(() => ['trendChart', 'trendChartLite'].includes(configWidget.value?.type))
const isCustomShapeConfig = computed(() => configWidget.value?.type === 'customShape')
const isScadaSvgConfig = computed(() => configWidget.value?.type === 'scadaSvg')
const isProcessConfig = computed(() => String(configWidget.value?.type || '').startsWith('process') || isCustomShapeConfig.value || isScadaSvgConfig.value)
const isConnectableConfig = computed(() =>
  configWidget.value &&
  (String(configWidget.value.type || '').startsWith('process') || configWidget.value.type === 'customShape' || configWidget.value.type === 'scadaSvg') &&
  configWidget.value.type !== 'processValueTag'
)
const selectedConnection = computed(() =>
  selectedConnectionId.value
    ? store.connections.find(connection => connection.id === selectedConnectionId.value) || null
    : null
)
const contextTargetTitle = computed(() => {
  if (contextMenu.targetType === 'application') return '应用模式'
  if (contextMenu.targetType === 'connection') return '连接线操作'
  if (contextMenu.targetType === 'widget') {
    const widget = store.widgets.find(item => item.id === contextMenu.targetId)
    return widget?.config?.title || typeLabel(widget?.type) || '组件操作'
  }
  return '画布操作'
})
const deleteWidgetTitle = computed(() => {
  if (!deleteWidgetTarget.value) return ''
  return deleteWidgetTarget.value.config?.title || typeLabel(deleteWidgetTarget.value.type)
})

const showVarSelect = computed(() => {
  if (!configWidget.value) return false
  return [
    'trendChart',
    'trendChartLite',
    'digitalDisplay',
    'valueColumn',
    'gauge',
    'button',
    'winccToggleButton',
    'stepperControl',
    'indicator',
    'statusCircle',
    'processPump',
    'processDosingMachine',
    'processValve',
    'processValveVertical',
    'processValueTag',
    'processCalcinerFurnace',
    'processCycloneSeparator',
    'processPreheaterTower',
    'processBagFilter',
    'processFanBlower',
    'processWinccFan',
    'processBurner',
    'processAluminaSilo',
    'processScrewConveyor',
    'processBucketElevator',
    'processGasValveSkid',
    'processHeatExchanger',
    'processKilnTube',
    'processStack',
    'processRotaryAirlock',
    'processDamper',
    'processSprayLance',
    'processMaterialChute',
    'processDoubleFlapValve'
  ].includes(configWidget.value.type) ||
    ((configWidget.value.type === 'customShape' || configWidget.value.type === 'scadaSvg') && Boolean(activeConfig.value.bindVariable))
})

function readVariableGroupMap() {
  try {
    const parsed = JSON.parse(localStorage.getItem('monitor-variable-groups-v1') || '{}')
    return parsed && typeof parsed === 'object' ? parsed : {}
  } catch {
    return {}
  }
}

const configVariableGroups = computed(() => {
  const customGroups = readVariableGroupMap()
  const groups = new Map()
  const query = variableBindingSearch.value.trim().toLocaleLowerCase()
  store.variables.forEach(v => {
    if (query && ![v.name, v.description, v.source, v.unit, v.id]
      .some(value => String(value || '').toLocaleLowerCase().includes(query))) return
    const label = customGroups[v.id] || v.source || '未分组'
    const key = `var-group:${label}`
    if (!groups.has(key)) groups.set(key, { key, label, items: [] })
    groups.get(key).items.push(v)
  })
  return [...groups.values()]
    .map(group => ({ ...group, items: [...group.items].sort((a, b) => String(a.name || '').localeCompare(String(b.name || ''), 'zh-CN')) }))
    .sort((a, b) => {
      if (a.label === '未分组') return 1
      if (b.label === '未分组') return -1
      return String(a.label).localeCompare(String(b.label), 'zh-CN')
    })
})

function isConfigVariableGroupCollapsed(key) {
  return Boolean(collapsedConfigVariableGroups[key]) && !variableBindingSearch.value.trim()
}

function toggleConfigVariableGroup(key) {
  collapsedConfigVariableGroups[key] = !collapsedConfigVariableGroups[key]
}

const visibleBindingVariableCount = computed(() =>
  configVariableGroups.value.reduce((total, group) => total + group.items.length, 0)
)
const selectedVarInfo = computed(() => {
  if (!configWidget.value || isTrend.value) return null
  const vid = activeConfig.value.varId
  if (vid == null) return null
  return store.variables.find(v => v.id === vid) || null
})

function getVarQuality(id) {
  if (id == null) return 'bad'
  const dp = store.realtimeData[id]
  return dp?.quality === 'GOOD' ? 'good' : (dp?.quality === 'UNCERTAIN' ? 'uncertain' : 'bad')
}

function normalizeRotationValue(value) {
  const number = Number(value)
  if (!Number.isFinite(number)) return 0
  return Math.round((((number % 360) + 360) % 360) * 10) / 10
}

function normalizeActiveRotation() {
  activeConfig.value.rotation = normalizeRotationValue(activeConfig.value.rotation)
  applyConfig()
}

function normalizeRotationStep() {
  const step = Number(activeConfig.value.rotationStep)
  activeConfig.value.rotationStep = Math.max(1, Math.min(180, Number.isFinite(step) ? Math.round(step) : 90))
  applyConfig()
}

function rotateActiveWidget(direction) {
  const step = Math.max(1, Math.min(180, Number(activeConfig.value.rotationStep) || 90))
  const current = Number(activeConfig.value.rotation) || 0
  activeConfig.value.rotation = normalizeRotationValue(current + step * direction)
  activeConfig.value.rotationStep = step
  applyConfig()
}

function resetActiveRotation() {
  activeConfig.value.rotation = 0
  applyConfig()
}

function toggleTrendVar(id) {
  const cfg = activeConfig.value
  const ids = Array.isArray(cfg.varIds) ? [...cfg.varIds] : []
  const idx = ids.indexOf(id)
  if (idx >= 0) ids.splice(idx, 1)
  else ids.push(id)
  cfg.varIds = ids
  applyConfig()
}

const PORT_CONFIG_SIDES = ['top', 'right', 'bottom', 'left']

function portSideLabel(side) {
  return ({ top: '上', right: '右', bottom: '下', left: '左' })[side] || side
}

function defaultPortSidesForType(type) {
  if (type === 'processPump') return ['left', 'right']
  if (type === 'processBagFilter') return ['right', 'left']
  if (type === 'processFanBlower') return ['left', 'right', 'top']
  if (type === 'processWinccFan') return ['top']
  if (type === 'processHeatExchanger') return ['top', 'bottom']
  if (type === 'processRotaryAirlock') return ['top']
  if (['processKilnTube', 'processDamper', 'processSprayLance', 'processMaterialChute'].includes(type)) return ['left', 'right']
  if (type === 'processStack') return ['bottom', 'left', 'right']
  return ['top', 'right', 'bottom', 'left']
}

function portConfigFromSide(side, offset = 50) {
  const ratio = Math.max(0, Math.min(100, Number(offset) || 50)) / 100
  return {
    id: `${side}-${Date.now()}-${Math.round(Math.random() * 1000)}`,
    side,
    x: side === 'left' ? 0 : (side === 'right' ? 1 : ratio),
    y: side === 'top' ? 0 : (side === 'bottom' ? 1 : ratio)
  }
}

function clampPortPercent(value, fallback = 50) {
  const number = Number(value)
  return Math.max(0, Math.min(100, Number.isFinite(number) ? number : fallback))
}

function nearestCustomPortSide(x, y) {
  const distances = [
    { side: 'top', distance: Math.abs(y) },
    { side: 'right', distance: Math.abs(100 - x) },
    { side: 'bottom', distance: Math.abs(100 - y) },
    { side: 'left', distance: Math.abs(x) }
  ]
  distances.sort((a, b) => a.distance - b.distance)
  return distances[0].side
}

function customPortConfigFromDefinitionPort(port, index = 0) {
  const offset = clampPortPercent(port?.offset, 50)
  const legacy = port?.side === 'top'
    ? { x: offset, y: 0 }
    : port?.side === 'bottom'
      ? { x: offset, y: 100 }
      : port?.side === 'left'
        ? { x: 0, y: offset }
        : { x: 100, y: offset }
  const x = clampPortPercent(port?.xPercent ?? port?.x, legacy.x)
  const y = clampPortPercent(port?.yPercent ?? port?.y, legacy.y)
  return {
    id: port?.id || `custom-port-${index + 1}`,
    side: ['top', 'right', 'bottom', 'left'].includes(port?.side) ? port.side : nearestCustomPortSide(x, y),
    x: x / 100,
    y: y / 100
  }
}

function defaultConnectionPortsForConfigWidget() {
  const widget = configWidget.value
  if (!widget) return []
  const definitionPorts = widget.type === 'customShape' && Array.isArray(activeConfig.value.definition?.ports)
    ? activeConfig.value.definition.ports.map(customPortConfigFromDefinitionPort)
    : []
  if (definitionPorts.length) return definitionPorts
  return defaultPortSidesForType(widget.type).map(side => portConfigFromSide(side, 50))
}

function ensureConnectionPortsDraft() {
  const cfg = activeConfig.value
  if (!Array.isArray(cfg.connectionPorts)) {
    cfg.connectionPorts = defaultConnectionPortsForConfigWidget()
  }
  return cfg.connectionPorts
}

function firstConfigPort(side) {
  const ports = Array.isArray(activeConfig.value.connectionPorts)
    ? activeConfig.value.connectionPorts
    : defaultConnectionPortsForConfigWidget()
  return ports.find(port => port.side === side) || null
}

function isConfigPortEnabled(side) {
  return Boolean(firstConfigPort(side))
}

function configPortOffset(side) {
  const port = firstConfigPort(side)
  if (!port) return 50
  const value = side === 'top' || side === 'bottom'
    ? Number(port.x) * 100
    : Number(port.y) * 100
  return Math.round(Math.max(0, Math.min(100, Number.isFinite(value) ? value : 50)))
}

function toggleConfigPort(side, enabled) {
  const ports = ensureConnectionPortsDraft()
  const next = ports.filter(port => port.side !== side)
  if (enabled) next.push(portConfigFromSide(side, 50))
  activeConfig.value.connectionPorts = next
  syncCustomShapeDefinitionFromRuntime()
  applyConfig()
}

function setConfigPortOffset(side, value) {
  const ports = ensureConnectionPortsDraft()
  let port = ports.find(item => item.side === side)
  if (!port) {
    port = portConfigFromSide(side, value)
    ports.push(port)
  }
  const ratio = Math.max(0, Math.min(100, Number(value) || 0)) / 100
  if (side === 'top' || side === 'bottom') port.x = ratio
  else port.y = ratio
  activeConfig.value.connectionPorts = [...ports]
  syncCustomShapeDefinitionFromRuntime()
  applyConfig()
}

function syncCustomShapeDefinitionFromRuntime() {
  if (configWidget.value?.type !== 'customShape') return
  const cfg = activeConfig.value
  const definition = cfg.definition || {}
  cfg.definition = {
    ...definition,
    bindVariable: Boolean(cfg.bindVariable),
    statusOnColor: cfg.statusOnColor || definition.statusOnColor || '#22c55e',
    statusOffColor: cfg.statusOffColor || definition.statusOffColor || '#ef4444',
    ports: (Array.isArray(cfg.connectionPorts) ? cfg.connectionPorts : []).map(port => ({
      id: port.id,
      side: port.side,
      offset: Math.round(Math.max(0, Math.min(100, (port.side === 'top' || port.side === 'bottom' ? Number(port.x) : Number(port.y)) * 100 || 50)))
    }))
  }
}

function syncCustomShapeRuntimeConfig() {
  syncCustomShapeDefinitionFromRuntime()
  applyConfig()
}

function syncConnectionsAfterPortConfig(widgetId, config) {
  const ports = Array.isArray(config?.connectionPorts) ? config.connectionPorts : []
  if (!widgetId || !ports.length) return
  const sideCount = ports.reduce((acc, port) => {
    if (PORT_CONFIG_SIDES.includes(port.side)) acc[port.side] = (acc[port.side] || 0) + 1
    return acc
  }, {})
  const anchorForSide = (side) => {
    const port = ports.find(item => item.side === side)
    const x = Number(port?.x)
    const y = Number(port?.y)
    if (!PORT_CONFIG_SIDES.includes(side) || !Number.isFinite(x) || !Number.isFinite(y)) return null
    return {
      x: Math.max(0, Math.min(1, x)),
      y: Math.max(0, Math.min(1, y)),
      side
    }
  }
  store.connections.forEach(connection => {
    const patch = {}
    if (connection.from === widgetId && connection.fromPort && !connection.fromAnchor?.free) {
      const anchor = anchorForSide(connection.fromPort)
      if (anchor) patch.fromAnchor = anchor
    }
    if (connection.to === widgetId && connection.toPort && !connection.toAnchor?.free) {
      const anchor = anchorForSide(connection.toPort)
      if (anchor) patch.toAnchor = anchor
    }
    if (connection.from === widgetId && connection.fromAnchor && !connection.fromAnchor.free && sideCount[connection.fromAnchor.side] <= 1 && !anchorForSide(connection.fromAnchor.side)) {
      patch.fromAnchor = null
    }
    if (connection.to === widgetId && connection.toAnchor && !connection.toAnchor.free && sideCount[connection.toAnchor.side] <= 1 && !anchorForSide(connection.toAnchor.side)) {
      patch.toAnchor = null
    }
    if (Object.keys(patch).length) store.updateConnection(connection.id, patch)
  })
}

function getVarInfo(id) {
  return store.variables.find(v => v.id === id) || null
}

const categories = ref([
  {
    key: 'process', label: '工艺设备库', description: '公共件与行业场景', icon: '工', open: true,
    groups: [
      {
        key: 'process-common',
        label: '公共工艺组件',
        description: '通用阀门、示牌和连接件',
        open: true,
        items: [
          { type: 'processValueTag',      label: '数据示牌',   icon: '数', color: '#a66a1f' },
          { type: 'processValve',         label: '横向阀门',   icon: '阀', color: '#16a34a' },
          { type: 'processValveVertical', label: '立式阀门',   icon: '阀', color: '#16a34a' },
          { type: 'processPipeEnd',       label: '管道端口',   icon: '口', color: '#14b8a6' },
          { type: 'processJunction',      label: '连接节点',   icon: '点', color: '#178447' },
        ]
      },
      {
        key: 'alumina-calcination',
        label: '氧化铝焙烧',
        description: '焙烧炉、旋风筒、收尘、风机和燃气系统',
        open: true,
        items: [
          { type: 'processCalcinerFurnace',  label: '焙烧炉',       icon: '炉', color: '#8b5e34' },
          { type: 'processCycloneSeparator', label: '旋风分离器',   icon: '旋', color: '#64748b' },
          { type: 'processPreheaterTower',   label: '预热/冷却器',  icon: '换', color: '#64748b' },
          { type: 'processBagFilter',        label: '布袋除尘器',   icon: '尘', color: '#7c8b97' },
          { type: 'processFanBlower',        label: '引风机',       icon: '风', color: '#15803d' },
          { type: 'processWinccFan',         label: '风机',    icon: '风', color: '#39d10f' },
          { type: 'processBurner',           label: '燃烧器',       icon: '燃', color: '#dc2626' },
          { type: 'processAluminaSilo',      label: '氧化铝料仓',   icon: '仓', color: '#8a9398' },
          { type: 'processScrewConveyor',    label: '螺旋输送机',   icon: '螺', color: '#475569' },
          { type: 'processBucketElevator',   label: '斗式提升机',   icon: '提', color: '#475569' },
          { type: 'processGasValveSkid',     label: '燃气阀组',     icon: '气', color: '#d99a1a' },
          { type: 'processStatusMatrix',     label: '状态灯盘',     icon: '灯', color: '#4f6fb8' },
          { type: 'processHeatExchanger',    label: '换热器',       icon: '换', color: '#64748b' },
          { type: 'processKilnTube',         label: '炉筒子',       icon: '筒', color: '#475569' },
          { type: 'processStack',            label: '烟囱/排气筒',  icon: '烟', color: '#94a3b8' },
          { type: 'processRotaryAirlock',    label: '星型下料阀',   icon: '星', color: '#16a34a' },
          { type: 'processDamper',           label: '风门挡板',     icon: '挡', color: '#16a34a' },
          { type: 'processSprayLance',       label: '喷枪',         icon: '喷', color: '#dc2626' },
          { type: 'processMaterialChute',    label: '斜溜槽',       icon: '槽', color: '#64748b' },
          { type: 'processDoubleFlapValve',  label: '双层翻板阀',   icon: '板', color: '#16a34a' },
        ]
      },
      {
        key: 'hanwang-scene',
        label: '罕王现场场景',
        description: '尾矿供水相关设备',
        open: true,
        items: [
          { type: 'processPump',          label: '水泵/气泵',  icon: '泵', color: '#178447' },
          { type: 'processDosingMachine', label: '加药机',     icon: '药', color: '#256f9c' },
          { type: 'processPool',          label: '水池',       icon: '池', color: '#0ea5b7' },
          { type: 'processDam',           label: '尾矿库',     icon: '库', color: '#111827' },
          { type: 'processTank',          label: '水罐',       icon: '罐', color: '#64748b' },
          { type: 'processMixer',         label: '浓密机',     icon: '机', color: '#0ea5e9' },
        ]
      },
    ]
  },
  {
    key: 'display', label: '数据展示', description: '曲线、数值和仪表', icon: '数', open: true,
    items: [
      { type: 'trendChart',     label: '趋势曲线',     icon: '📈', color: '#4f6fb8' },
      { type: 'trendChartLite', label: '简洁趋势曲线', icon: '📉', color: '#3b82f6' },
      { type: 'digitalDisplay', label: '数值显示',     icon: '🔢', color: '#2f8f63' },
      { type: 'valueColumn',    label: '液柱显示',     icon: '柱', color: '#ec4899' },
      { type: 'gauge',          label: '仪表盘',       icon: '⭕', color: '#a66a1f' },
    ]
  },
  {
    key: 'control', label: '控制与状态', description: '下发控制和状态反馈', icon: '控', open: true,
    items: [
      { type: 'button',   label: '控制按钮',   icon: '🔘', color: '#c2414b' },
      { type: 'winccToggleButton', label: '状态切换按钮', icon: '切', color: '#18c93a' },
      { type: 'stepperControl', label: '加减调节', icon: '±', color: '#64748b' },
      { type: 'indicator', label: '状态指示',   icon: '💡', color: '#8250df' },
      { type: 'statusCircle', label: '状态圆点', icon: '●', color: '#22c55e' },
    ]
  },
  {
    key: 'info', label: '文本与标注', description: '画面文字说明', icon: '文', open: true,
    items: [
      { type: 'label',     label: '文本标签',   icon: '📝', color: '#656d76' },
      { type: 'frameBox',  label: '边框容器',   icon: '▣', color: '#4f6fb8' },
    ]
  },
])

const libraryQuery = computed(() => librarySearch.value.trim().toLowerCase())
const hasLibrarySearch = computed(() => libraryQuery.value.length > 0)
const customLibraryOpen = ref(true)
const scadaAssetLibraryOpen = ref(true)
const libraryCategories = computed(() => {
  const importedItems = scadaAssetLibrary.value.map(asset => ({
    type: 'scadaSvg',
    label: asset.name,
    icon: '图',
    color: '#0ea5e9',
    scadaAsset: asset
  }))
  const customItems = store.customComponents.map(component => ({
    type: 'customShape',
    label: component.name,
    icon: '自',
    color: '#2563eb',
    customComponentId: component.id,
    customComponent: component
  }))
  const extraCategories = []
  if (importedItems.length) {
    extraCategories.push({
      key: 'imported-scada-assets',
      label: '导入组件库',
      description: '本浏览器保存的 SVG/图片组件',
      icon: '图',
      open: scadaAssetLibraryOpen.value,
      items: importedItems
    })
  }
  if (customItems.length) {
    extraCategories.push({
      key: 'custom-components',
      label: '自定义组件',
      description: '组件配置器保存的模块',
      icon: '自',
      open: customLibraryOpen.value,
      items: customItems
    })
  }
  if (!extraCategories.length) return categories.value
  return [
    ...extraCategories,
    ...categories.value
  ]
})
const visibleCategories = computed(() => {
  const q = libraryQuery.value
  if (!q) return libraryCategories.value

  return libraryCategories.value.map(cat => {
    if (Array.isArray(cat.groups)) {
      const groups = cat.groups.map(group => {
        const items = group.items.filter(item => matchesLibraryText(q, item.label, item.type, item.icon))
        return items.length ? { ...group, source: group, items } : null
      }).filter(Boolean)
      return groups.length ? { ...cat, source: cat, groups } : null
    }
    const items = cat.items.filter(item => matchesLibraryText(q, item.label, item.type, item.icon))
    return items.length ? { ...cat, source: cat, items } : null
  }).filter(Boolean)
})

function matchesLibraryText(query, ...values) {
  return values.some(value => String(value || '').toLowerCase().includes(query))
}

function isLibraryCategoryOpen(cat) {
  if ((cat.source || cat).key === 'custom-components') return hasLibrarySearch.value || customLibraryOpen.value
  if ((cat.source || cat).key === 'imported-scada-assets') return hasLibrarySearch.value || scadaAssetLibraryOpen.value
  return hasLibrarySearch.value || Boolean((cat.source || cat).open)
}

function toggleLibraryCategory(cat) {
  const source = cat.source || cat
  if (source.key === 'custom-components') {
    customLibraryOpen.value = !customLibraryOpen.value
    return
  }
  if (source.key === 'imported-scada-assets') {
    scadaAssetLibraryOpen.value = !scadaAssetLibraryOpen.value
    return
  }
  source.open = !source.open
}

function isLibraryGroupOpen(group) {
  return hasLibrarySearch.value || Boolean((group.source || group).open)
}

function toggleLibraryGroup(group) {
  const source = group.source || group
  source.open = !source.open
}

function categoryItemCount(cat) {
  if (Array.isArray(cat?.groups)) {
    return cat.groups.reduce((sum, group) => sum + (Array.isArray(group.items) ? group.items.length : 0), 0)
  }
  return Array.isArray(cat?.items) ? cat.items.length : 0
}

function customShapeConfigFromDefinition(definition, label) {
  if (!definition) return null
  return {
    title: label || definition.name,
    label: label || definition.name,
    titleFontSize: 12,
    labelFontSize: 12,
    labelOffsetX: 0,
    labelOffsetY: 0,
    varId: null,
    bindVariable: Boolean(definition.bindVariable),
    statusOnColor: definition.statusOnColor || '#22c55e',
    statusOffColor: definition.statusOffColor || '#ef4444',
    definition,
    connectionPorts: (definition.ports || []).map(customPortConfigFromDefinitionPort)
  }
}

function readScadaAssetLibrary() {
  try {
    const parsed = JSON.parse(localStorage.getItem(SCADA_ASSET_LIBRARY_KEY) || '[]')
    if (!Array.isArray(parsed)) return []
    return parsed
      .filter(asset => asset && typeof asset.name === 'string' && typeof asset.path === 'string' && asset.path.startsWith('data:image/'))
      .slice(0, 12)
      .map(asset => ({
        id: String(asset.id || `asset-${Date.now()}`),
        name: asset.name.slice(0, 80),
        type: 'image',
        path: asset.path,
        width: Math.max(1, Number(asset.width) || WIDGET_TYPES.scadaSvg.defaultW),
        height: Math.max(1, Number(asset.height) || WIDGET_TYPES.scadaSvg.defaultH)
      }))
  } catch {
    return []
  }
}

function persistScadaAssetLibrary() {
  try {
    localStorage.setItem(SCADA_ASSET_LIBRARY_KEY, JSON.stringify(scadaAssetLibrary.value))
    return true
  } catch {
    return false
  }
}

function addScadaAssetToLibrary(asset) {
  const existing = scadaAssetLibrary.value.find(item => item.path === asset.path)
  if (existing) return existing
  const next = [asset, ...scadaAssetLibrary.value].slice(0, 12)
  const previous = scadaAssetLibrary.value
  scadaAssetLibrary.value = next
  if (!persistScadaAssetLibrary()) {
    scadaAssetLibrary.value = previous
    return null
  }
  return asset
}

function requestDeleteScadaAsset(asset) {
  const id = String(asset?.id || '')
  if (!id) return
  const usedOnCanvas = store.widgets.filter(widget => String(widget?.config?.asset?.id || '') === id).length
  deleteScadaAssetTarget.value = { asset, usedOnCanvas }
}

function confirmDeleteScadaAsset() {
  const target = deleteScadaAssetTarget.value
  const asset = target?.asset
  const id = String(asset?.id || '')
  if (!id) {
    deleteScadaAssetTarget.value = null
    return
  }
  const previous = scadaAssetLibrary.value
  scadaAssetLibrary.value = previous.filter(item => String(item.id) !== id)
  if (!persistScadaAssetLibrary()) {
    scadaAssetLibrary.value = previous
    showConnectionNotice('删除组件库项失败：浏览器存储不可用')
    return
  }
  deleteScadaAssetTarget.value = null
  showConnectionNotice(`已从导入组件库删除“${asset.name}”`)
}

function previewConfigForType(type, label, customDefinition = null, scadaAsset = null) {
  if (type === 'customShape' && customDefinition) {
    return customShapeConfigFromDefinition(customDefinition, label)
  }
  if (type === 'scadaSvg' && scadaAsset) {
    return { title: label, label, hideName: true, asset: scadaAsset, preserveAspectRatio: 'meet', bindVariable: false, varId: null }
  }
  const base = { title: label, label, titleFontSize: 12, labelFontSize: 12 }
  if (type === 'button') return { ...base, buttonText: label, writeValue: 1 }
  if (type === 'winccToggleButton') return { ...base, buttonText: label, writeValue: 1, pressed: false, activeColor: '#18c93a', inactiveTopColor: '#ffffff', inactiveBottomColor: '#e5e7eb', exclusiveEnabled: false, exclusivePeerIds: [], exclusiveLabel: '' }
  if (type === 'stepperControl') return { ...base, defaultValue: 0, min: 0, max: 100, step: 1, decimals: 0, unit: '', minusText: '-', plusText: '+' }
  if (type === 'label') return { ...base, text: label, fontSize: 16, color: '#344054', align: 'center', verticalAlign: 'center', background: 'transparent', borderColor: 'transparent', borderWidth: 0 }
  if (type === 'frameBox') return { ...base, hideName: false, background: '#ffffff', backgroundOpacity: 0, borderColor: '#4f6fb8', borderWidth: 2, borderRadius: 8, borderStyle: 'solid' }
  if (type === 'digitalDisplay' || type === 'gauge') return { ...base, min: 0, max: 100, unit: '', decimals: 1 }
  if (type === 'valueColumn') return { ...base, min: 0, max: 100, unit: '', decimals: 1, valuePosition: 'right', barColor: '#ec4899', trackColor: '#111827', scaleColor: '#98a2b3', textColor: '#f8fafc' }
  if (type === 'trendChart' || type === 'trendChartLite') return { ...base, varIds: [], timeWindow: 60 }
  if (type === 'indicator') return { ...base, threshold: 50 }
  if (type === 'statusCircle') return { ...base, hideName: true, threshold: 0 }
  if (type === 'processValueTag') return { ...base, unit: '', decimals: 1, valueFontSize: 16 }
  if (type === 'processPool' || type === 'processTank' || type === 'processMixer' || type === 'processAluminaSilo') return { ...base, fill: 65 }
  if (type === 'processPipeEnd') return { ...base, direction: 'right' }
  if (type === 'processPump') return { ...base, flipX: false, flipY: false }
  if (type === 'processValve' || type === 'processValveVertical' || type === 'processDosingMachine') return { ...base, normallyOpen: true }
  if ([
    'processCalcinerFurnace',
    'processCycloneSeparator',
    'processPreheaterTower',
    'processBagFilter',
    'processFanBlower',
    'processWinccFan',
    'processBurner',
    'processScrewConveyor',
    'processBucketElevator',
    'processGasValveSkid',
    'processHeatExchanger',
    'processKilnTube',
    'processStack',
    'processRotaryAirlock',
    'processDamper',
    'processSprayLance',
    'processMaterialChute',
    'processDoubleFlapValve'
  ].includes(type)) return { ...base, varId: null }
  return base
}

function widgetDefaultSize(type) {
  const def = WIDGET_TYPES[type] || {}
  return {
    w: Number(def.defaultW) || 140,
    h: Number(def.defaultH) || 80
  }
}

function updateLibraryPreviewPosition(event) {
  const previewW = 246
  const previewH = 208
  let x = event.clientX + 18
  let y = event.clientY - 32
  if (x + previewW > window.innerWidth - 10) x = event.clientX - previewW - 18
  y = Math.max(10, Math.min(y, window.innerHeight - previewH - 10))
  libraryPreview.x = Math.round(x)
  libraryPreview.y = Math.round(y)
}

function showLibraryPreview(item, event) {
  libraryPreview.item = item
  libraryPreview.open = true
  updateLibraryPreviewPosition(event)
}

function moveLibraryPreview(event) {
  if (!libraryPreview.open) return
  updateLibraryPreviewPosition(event)
}

function hideLibraryPreview() {
  libraryPreview.open = false
  libraryPreview.item = null
}

function onDragStart(event, item) {
  libraryDragging.value = true
  hideLibraryPreview()
  event.dataTransfer.setData('widget-type', item.type)
  event.dataTransfer.setData('text/plain', item.type)
  event.dataTransfer.setData('application/json', JSON.stringify({
    type: item.type,
    customComponentId: item.customComponentId || null,
    scadaAssetId: item.scadaAsset?.id || null
  }))
  if (item.customComponentId) event.dataTransfer.setData('custom-component-id', item.customComponentId)
  if (item.scadaAsset?.id) event.dataTransfer.setData('scada-asset-id', item.scadaAsset.id)
  event.dataTransfer.effectAllowed = 'copy'
}

function onLibraryDragEnd() {
  window.setTimeout(() => { libraryDragging.value = false }, 0)
}

function onLibraryItemPointerUp(event, item) {
  if (event.button !== 0 || libraryDragging.value) return
  lastLibraryActivationAt = Date.now()
  addToCanvas(item)
}

function onLibraryItemClick(event, item) {
  if (libraryDragging.value || Date.now() - lastLibraryActivationAt < 350) return
  addToCanvas(item)
}

function canvasCenterPoint() {
  const area = canvasAreaRef.value
  if (!area) return { x: 0, y: 0 }
  const rect = area.getBoundingClientRect()
  return canvasPointFromEvent({
    clientX: rect.left + rect.width / 2,
    clientY: rect.top + rect.height / 2
  })
}

function addToCanvas(item) {
  const customComponent = item.customComponent || findCustomComponent(item.customComponentId)
  const scadaAsset = item.scadaAsset || null
  const size = scadaAsset
    ? { w: scadaAsset.width, h: scadaAsset.height }
    : customComponent
    ? { w: customComponent.width, h: customComponent.height }
    : widgetDefaultSize(item.type)
  const point = canvasCenterPoint()
  const widget = store.addWidget(item.type, Math.round(point.x - size.w / 2), Math.round(point.y - size.h / 2), {
    customComponent,
    w: item.scadaAsset?.width,
    h: item.scadaAsset?.height,
    config: item.scadaAsset ? previewConfigForType('scadaSvg', item.label, null, item.scadaAsset) : undefined
  })
  if (widget) {
    openConfig(widget.id)
    showConnectionNotice(`已添加“${item.label || typeLabel(item.type)}”`)
  } else {
    showConnectionNotice(`组件“${item.label || item.type}”添加失败`)
  }
}

function encodeUtf8Base64(text) {
  const bytes = new TextEncoder().encode(text)
  let binary = ''
  for (let offset = 0; offset < bytes.length; offset += 0x8000) {
    binary += String.fromCharCode(...bytes.subarray(offset, Math.min(bytes.length, offset + 0x8000)))
  }
  return btoa(binary)
}

async function scadaAssetDataUrl(file) {
  const isSvg = file.type === 'image/svg+xml' || /\.svg$/i.test(file.name)
  if (!isSvg) return await new Promise((resolve, reject) => {
    const reader = new FileReader()
    reader.onload = () => resolve(String(reader.result || ''))
    reader.onerror = () => reject(new Error('读取图片失败'))
    reader.readAsDataURL(file)
  })

  const source = await file.text()
  const document = new DOMParser().parseFromString(source, 'image/svg+xml')
  if (document.querySelector('parsererror') || document.documentElement?.nodeName.toLowerCase() !== 'svg') {
    throw new Error('不是有效的 SVG 文件')
  }
  document.querySelectorAll('script, foreignObject, iframe, object, embed').forEach(node => node.remove())
  document.querySelectorAll('*').forEach(node => {
    Array.from(node.attributes).forEach(attribute => {
      const name = attribute.name.toLowerCase()
      const value = attribute.value.trim().toLowerCase()
      if (name.startsWith('on') ||
          ((name === 'href' || name === 'xlink:href') && value && !value.startsWith('#') && !value.startsWith('data:'))) {
        node.removeAttribute(attribute.name)
      }
    })
  })
  return `data:image/svg+xml;base64,${encodeUtf8Base64(new XMLSerializer().serializeToString(document.documentElement))}`
}

async function importScadaAsset(event) {
  const file = event.target?.files?.[0]
  if (event.target) event.target.value = ''
  if (!file) return
  if (file.size > 768 * 1024) {
    showConnectionNotice('导入文件不能超过 768 KiB，避免工程数据过大')
    return
  }
  const validImage = file.type.startsWith('image/') || /\.svg$/i.test(file.name)
  if (!validImage) {
    showConnectionNotice('只支持 SVG、PNG、JPEG、WEBP 或 GIF 图片')
    return
  }
  try {
    const path = await scadaAssetDataUrl(file)
    const label = file.name.replace(/\.[^.]+$/, '').trim() || '导入组件'
    const asset = { id: `asset-${Date.now()}`, name: label, type: 'image', path, width: 180, height: 140 }
    const storedAsset = addScadaAssetToLibrary(asset)
    const offset = 56 + (store.widgets.length % 12) * 28
    const widget = store.addWidget('scadaSvg', offset, offset, {
      w: asset.width,
      h: asset.height,
      config: previewConfigForType('scadaSvg', label, null, asset)
    })
    if (!widget) throw new Error('无法创建导入组件')
    selectWidget(widget.id)
    showConnectionNotice(storedAsset
      ? `已导入「${label}」并加入组件库，可在属性面板绑定变量`
      : `已导入「${label}」，但组件库空间不足，未保存为复用项`)
  } catch (error) {
    showConnectionNotice(error?.message || '导入组件失败')
  }
}

function readDroppedLibraryItem(dataTransfer) {
  const directType = dataTransfer.getData('widget-type')
  const customComponentId = dataTransfer.getData('custom-component-id')
  const scadaAssetId = dataTransfer.getData('scada-asset-id')
  if (directType) return { type: directType, customComponentId, scadaAssetId }

  try {
    const payload = JSON.parse(dataTransfer.getData('application/json') || '{}')
    if (payload?.type) return {
      type: payload.type,
      customComponentId: payload.customComponentId || customComponentId || '',
      scadaAssetId: payload.scadaAssetId || scadaAssetId || ''
    }
  } catch {}

  const textType = dataTransfer.getData('text/plain')
  return textType ? { type: textType, customComponentId, scadaAssetId } : null
}

function onDrop(e) {
  const dropped = readDroppedLibraryItem(e.dataTransfer)
  if (!dropped?.type) return
  if (!WIDGET_TYPES[dropped.type]) {
    showConnectionNotice(`未知组件类型：${dropped.type}`)
    return
  }
  const customComponent = findCustomComponent(dropped.customComponentId)
  const scadaAsset = findScadaAsset(dropped.scadaAssetId)
  const point = canvasPointFromEvent(e)
  const size = scadaAsset
    ? { w: scadaAsset.width, h: scadaAsset.height }
    : customComponent
    ? { w: customComponent.width, h: customComponent.height }
    : widgetDefaultSize(dropped.type)
  const widget = store.addWidget(dropped.type, Math.round(point.x - size.w / 2), Math.round(point.y - size.h / 2), {
    customComponent,
    w: scadaAsset?.width,
    h: scadaAsset?.height,
    config: scadaAsset ? previewConfigForType('scadaSvg', scadaAsset.name, null, scadaAsset) : undefined
  })
  if (widget) {
    selectWidget(widget.id)
    showConnectionNotice(`已添加“${typeLabel(dropped.type)}”`)
  }
}

function findCustomComponent(id) {
  if (!id) return null
  return store.customComponents.find(component => component.id === id) || null
}

function findScadaAsset(id) {
  if (!id) return null
  return scadaAssetLibrary.value.find(asset => asset.id === id) || null
}

// ============ 画布平移 (transform) & 缩放 ============
const panX = ref(0)
const panY = ref(0)
const zoom = ref(1)
provide('canvasZoom', zoom)
const isZooming = ref(false)
provide('isCanvasZooming', isZooming)
const zoomRefreshTick = ref(0)
provide('zoomRefreshTick', zoomRefreshTick)
const activeWidgetTransformId = ref(null)
const connectionRenderEpoch = ref(0)
const isPanning = ref(false)
const panStart = ref({ x: 0, y: 0 })
const panOrigin = ref({ x: 0, y: 0 })
let wheelFrame = 0
let wheelIdleTimer = null
const pendingWheel = { deltaY: 0, clientX: 0, clientY: 0 }
let panFrame = 0
let pendingPan = null
const EDITOR_CANVAS_ORIGIN_X = 4800
const EDITOR_CANVAS_ORIGIN_Y = 3600
const EDITOR_CANVAS_PADDING = 1200
const EDITOR_CANVAS_MIN_WIDTH = 9600
const EDITOR_CANVAS_MIN_HEIGHT = 7200

function beginWidgetTransform(widgetId) {
  activeWidgetTransformId.value = widgetId || null
}

function endWidgetTransform(widgetId) {
  if (!widgetId || activeWidgetTransformId.value === widgetId) {
    activeWidgetTransformId.value = null
  }
  connectionRenderEpoch.value += 1
}

provide('beginWidgetTransform', beginWidgetTransform)
provide('endWidgetTransform', endWidgetTransform)

function editorCanvasX(value) {
  return (Number(value) || 0) + editorCanvasBounds.value.offsetX
}

function editorCanvasY(value) {
  return (Number(value) || 0) + editorCanvasBounds.value.offsetY
}

function editorCanvasPoint(point = {}) {
  return {
    x: editorCanvasX(point.x),
    y: editorCanvasY(point.y)
  }
}

function editorCanvasPoints(points = []) {
  return points.map(point => editorCanvasPoint(point))
}

const editorViewportStyle = computed(() => ({
  transform: `translate(${panX.value}px, ${panY.value}px) scale(${zoom.value})`,
  width: `${editorCanvasBounds.value.width}px`,
  height: `${editorCanvasBounds.value.height}px`
}))

const canvasAreaStyle = computed(() => {
  const z = Number(zoom.value) || 1
  const grid = Math.max(8, 20 * z)
  const x = ((panX.value % grid) + grid) % grid
  const y = ((panY.value % grid) + grid) % grid
  return {
    backgroundSize: `${grid}px ${grid}px`,
    backgroundPosition: `${x - 1}px ${y - 1}px`
  }
})

const minimapViewport = computed(() => ({
  panX: panX.value,
  panY: panY.value,
  zoom: zoom.value,
  width: canvasAreaSize.value.width,
  height: canvasAreaSize.value.height,
  canvasOffset: editorCanvasOffset.value
}))

function updateCanvasAreaSize() {
  const area = canvasAreaRef.value
  if (!area) return
  const width = Math.max(0, Math.round(area.clientWidth || 0))
  const height = Math.max(0, Math.round(area.clientHeight || 0))
  if (canvasAreaSize.value.width !== width || canvasAreaSize.value.height !== height) {
    canvasAreaSize.value = { width, height }
  }
}

function navigateCanvasOverview(point) {
  if (!point) return
  updateCanvasAreaSize()
  const area = canvasAreaRef.value
  const width = canvasAreaSize.value.width || area?.clientWidth || 0
  const height = canvasAreaSize.value.height || area?.clientHeight || 0
  const targetX = Number(point.x)
  const targetY = Number(point.y)
  if (!Number.isFinite(targetX) || !Number.isFinite(targetY) || width <= 0 || height <= 0) return
  panX.value = width / 2 - targetX * zoom.value
  panY.value = height / 2 - targetY * zoom.value
}

function connectedPortAlignment(widgetId, nextX, nextY) {
  const widget = widgetMap.value[widgetId]
  if (!widget) return { x: nextX, y: nextY, guide: null }
  const candidate = { ...widget, x: nextX, y: nextY }
  let best = null

  const considerAlignment = (axis, delta, guideValue, priority = 0) => {
    const distance = Math.abs(delta)
    if (distance > WIDGET_ALIGNMENT_SNAP) return
    const score = distance - priority * 2
    if (best && score >= best.score) return
    best = {
      score,
      distance,
      x: axis === 'x' ? nextX + delta : nextX,
      y: axis === 'y' ? nextY + delta : nextY,
      guide: axis === 'x'
        ? { orientation: 'vertical', value: guideValue }
        : { orientation: 'horizontal', value: guideValue }
    }
  }

  store.connections.forEach(conn => {
    const isFrom = conn.from === widgetId
    const isTo = conn.to === widgetId
    if (!isFrom && !isTo) return

    const movingPort = isFrom ? conn.fromPort : conn.toPort
    const fixedPort = isFrom ? conn.toPort : conn.fromPort
    const movingAnchor = isFrom ? conn.fromAnchor : conn.toAnchor
    const fixedAnchor = isFrom ? conn.toAnchor : conn.fromAnchor
    const fixedWidget = isFrom ? widgetMap.value[conn.to] : widgetMap.value[conn.from]
    if (!movingPort || !fixedPort || !fixedWidget) return

    const movingPoint = connectionWidgetAnchor(candidate, fixedWidget, movingPort, movingAnchor)
    const fixedPoint = connectionWidgetAnchor(fixedWidget, candidate, fixedPort, fixedAnchor)
    const movingExit = offsetPointByPort(movingPoint, movingPort)
    const fixedExit = offsetPointByPort(fixedPoint, fixedPort)
    const movingHorizontal = isHorizontalPort(movingPort)
    const fixedHorizontal = isHorizontalPort(fixedPort)
    const movingVertical = !movingHorizontal && Boolean(portDirection(movingPort))
    const fixedVertical = !fixedHorizontal && Boolean(portDirection(fixedPort))
    const movingDirection = portDirection(movingPort)
    const fixedDirection = portDirection(fixedPort)
    const isOppositeHorizontal = movingHorizontal && fixedHorizontal && movingDirection?.x === -(fixedDirection?.x || 0)
    const isOppositeVertical = movingVertical && fixedVertical && movingDirection?.y === -(fixedDirection?.y || 0)

    if (movingHorizontal && fixedHorizontal) {
      considerAlignment('y', fixedPoint.y - movingPoint.y, fixedPoint.y, isOppositeHorizontal ? 2 : 1)
      considerAlignment('y', fixedExit.y - movingExit.y, fixedExit.y, isOppositeHorizontal ? 3 : 1)
    }

    if (movingVertical && fixedVertical) {
      considerAlignment('x', fixedPoint.x - movingPoint.x, fixedPoint.x, isOppositeVertical ? 2 : 1)
      considerAlignment('x', fixedExit.x - movingExit.x, fixedExit.x, isOppositeVertical ? 3 : 1)
    }
  })

  return best || { x: nextX, y: nextY, guide: null }
}

function alignWidgetDrag(widgetId, nextX, nextY) {
  const aligned = connectedPortAlignment(widgetId, nextX, nextY)
  orthogonalGuide.value = aligned.guide
  activeAlignmentHandle.value = aligned.guide ? { type: 'widget', widgetId } : null
  return { x: aligned.x, y: aligned.y }
}

function clearWidgetDragAlignment(widgetId) {
  if (!widgetId || activeAlignmentHandle.value?.type === 'widget') {
    clearAlignmentFeedback()
  }
}

provide('alignWidgetDrag', alignWidgetDrag)
provide('clearWidgetDragAlignment', clearWidgetDragAlignment)

const orthogonalGuideLineAttrs = computed(() => {
  if (!orthogonalGuide.value) return {}
  if (orthogonalGuide.value.orientation === 'horizontal') {
    return {
      x1: 0,
      y1: editorCanvasY(orthogonalGuide.value.value),
      x2: editorCanvasBounds.value.width,
      y2: editorCanvasY(orthogonalGuide.value.value)
    }
  }
  return {
    x1: editorCanvasX(orthogonalGuide.value.value),
    y1: 0,
    x2: editorCanvasX(orthogonalGuide.value.value),
    y2: editorCanvasBounds.value.height
  }
})

const rubberBandPath = computed(() => {
  if (!rubberBand.value) return ''
  return pointsPath(editorCanvasPoints(rubberBand.value.points || [rubberBand.value.start, rubberBand.value.end]))
})

const widgetMap = computed(() => {
  const map = {}
  store.widgets.forEach(w => { map[w.id] = w })
  return map
})

const widgetTopologySignature = computed(() =>
  store.widgets
    .map(widget => [
      widget.id,
      Math.round(Number(widget.x) || 0),
      Math.round(Number(widget.y) || 0),
      Math.round(Number(widget.w) || 0),
      Math.round(Number(widget.h) || 0)
    ].join(':'))
    .join('|')
)

const widgetPortConfigSignature = computed(() =>
  store.widgets
    .map(widget => {
      const ports = Array.isArray(widget.config?.connectionPorts) ? widget.config.connectionPorts : []
      return [
        widget.id,
        ...ports.map(port => [
          port.side || '',
          Number.isFinite(Number(port.x)) ? Number(port.x).toFixed(5) : '',
          Number.isFinite(Number(port.y)) ? Number(port.y).toFixed(5) : ''
        ].join(','))
      ].join(':')
    })
    .join('|')
)

function widgetPortSideEnabled(widget, side) {
  if (!widget || !PORT_NAMES.includes(side)) return true
  const ports = Array.isArray(widget.config?.connectionPorts) ? widget.config.connectionPorts : null
  if (!ports) return true
  return ports.some(port => port.side === side)
}

function connectionEnabledByPorts(conn) {
  if (Array.isArray(conn.points) && conn.points.length >= 2 && (!conn.from || !conn.to)) return true
  const fromWidget = widgetMap.value[conn.from]
  const toWidget = widgetMap.value[conn.to]
  if (conn.fromPort && !conn.fromAnchor?.free && !widgetPortSideEnabled(fromWidget, conn.fromPort)) return false
  if (conn.toPort && !conn.toAnchor?.free && !widgetPortSideEnabled(toWidget, conn.toPort)) return false
  return true
}

const visibleConnections = computed(() =>
  store.connections.filter(conn =>
    connectionEnabledByPorts(conn) &&
    (
      (Array.isArray(conn.points) && conn.points.length >= 2) ||
      (widgetMap.value[conn.from] && widgetMap.value[conn.to])
    )
  )
)

const renderedConnectionBaseCache = new Map()

function pointListSignature(points) {
  if (!Array.isArray(points) || !points.length) return ''
  return points
    .map(point => `${Math.round(Number(point.x) || 0)},${Math.round(Number(point.y) || 0)}`)
    .join('|')
}

function anchorSignature(anchor) {
  if (!anchor) return ''
  return `${Number(anchor.x).toFixed(5)},${Number(anchor.y).toFixed(5)},${anchor.side || ''},${anchor.free ? 'free' : 'fixed'}`
}

function buildConnectionRenderSignature(conn) {
  return [
    connectionRenderEpoch.value,
    widgetTopologySignature.value,
    widgetPortConfigSignature.value,
    conn.from || '',
    conn.to || '',
    conn.fromPort || '',
    conn.toPort || '',
    anchorSignature(conn.fromAnchor),
    anchorSignature(conn.toAnchor),
    conn.manualRoute ? '1' : '0',
    conn.routeMode || '',
    Number.isFinite(Number(conn.bendX)) ? Math.round(Number(conn.bendX)) : '',
    Number.isFinite(Number(conn.bendY)) ? Math.round(Number(conn.bendY)) : '',
    pointListSignature(conn.points),
    pointListSignature(conn.waypoints)
  ].join(';')
}

function pruneRenderedConnectionCache(validConnections) {
  const validIds = new Set(validConnections.map(conn => conn.id))
  Array.from(renderedConnectionBaseCache.keys()).forEach(id => {
    if (!validIds.has(id)) renderedConnectionBaseCache.delete(id)
  })
}

function buildConnectionWaypointHandles(conn) {
  return (
    Array.isArray(conn.points) && conn.points.length > 2
      ? conn.points.slice(1, -1).map((point, index) => ({
          index,
          x: Number(point.x),
          y: Number(point.y)
        }))
      : (Array.isArray(conn.waypoints)
          ? conn.waypoints.map((point, index) => ({
              index,
              x: Number(point.x),
              y: Number(point.y)
            }))
          : [])
  ).filter(point => Number.isFinite(point.x) && Number.isFinite(point.y))
}

function buildConnectionInsertHandles(points) {
  if (points.length < 2) return []
  const handles = []
  for (let i = 0; i < points.length - 1; i++) {
    const a = points[i]
    const b = points[i + 1]
    const isHorizontal = Math.abs(b.y - a.y) <= 0.001
    const isVertical = Math.abs(b.x - a.x) <= 0.001
    if (!isHorizontal && !isVertical) continue
    const length = Math.hypot(b.x - a.x, b.y - a.y)
    if (length < 44) continue
    handles.push({
      segmentIndex: i,
      orientation: isHorizontal ? 'horizontal' : 'vertical',
      x: a.x + (b.x - a.x) / 2,
      y: a.y + (b.y - a.y) / 2
    })
  }
  return handles
}

function buildRenderedConnectionBase(conn) {
  let route = null
  let points = []

  if (Array.isArray(conn.points) && conn.points.length >= 2) {
    points = compactConnectionPoints(conn.points.map(p => ({
      x: Number(p.x),
      y: Number(p.y)
    })))
  } else {
    const fromWidget = widgetMap.value[conn.from]
    const toWidget = widgetMap.value[conn.to]
    route = routeForWidgets(fromWidget, toWidget, conn)
    points = compactConnectionPoints(route.points || [])
  }

  const handleVisible = !(
    Array.isArray(conn.points) && conn.points.length >= 2
  ) && !(
    Array.isArray(conn.waypoints) && conn.waypoints.length > 0
  ) && Boolean(route) && (route.mode === 'hv' || route.mode === 'vh')

  return {
    connection: conn,
    points,
    path: pointsPath(points),
    handle: route?.handle || { x: 0, y: 0 },
    handleVisible
  }
}

function getRenderedConnectionBase(conn) {
  const cacheEntry = renderedConnectionBaseCache.get(conn.id)
  const activeWidgetId = activeWidgetTransformId.value
  const freezeUnrelated = Boolean(activeWidgetId) && conn.from !== activeWidgetId && conn.to !== activeWidgetId
  if (freezeUnrelated && cacheEntry?.rendered) return cacheEntry.rendered

  const signature = buildConnectionRenderSignature(conn)
  if (cacheEntry?.signature === signature && cacheEntry.rendered) return cacheEntry.rendered

  const rendered = buildRenderedConnectionBase(conn)
  renderedConnectionBaseCache.set(conn.id, { signature, rendered })
  return rendered
}

const renderedConnections = computed(() => {
  pruneRenderedConnectionCache(visibleConnections.value)
  return visibleConnections.value.map(conn => {
    const base = getRenderedConnectionBase(conn)
    const isSelected = selectedConnectionId.value === conn.id
    const points = editorCanvasPoints(base.points)
    return {
      ...base,
      connection: conn,
      points,
      path: pointsPath(points),
      handle: editorCanvasPoint(base.handle),
      waypointHandles: isSelected
        ? buildConnectionWaypointHandles(conn).map(handle => ({
            ...handle,
            x: editorCanvasX(handle.x),
            y: editorCanvasY(handle.y)
          }))
        : [],
      insertHandles: isSelected ? buildConnectionInsertHandles(points) : []
    }
  })
})

const applicationRenderedConnections = computed(() =>
  visibleConnections.value.map(conn => ({
    connection: conn,
    path: applicationConnectionPath(conn)
  }))
)

const applicationBounds = computed(() => {
  if (store.widgets.length === 0) {
    return { minX: 0, minY: 0, width: 800, height: 500, margin: 0 }
  }
  const extent = { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity }
  store.widgets.forEach(widget => {
    extent.minX = Math.min(extent.minX, Number(widget.x) || 0)
    extent.minY = Math.min(extent.minY, Number(widget.y) || 0)
    extent.maxX = Math.max(extent.maxX, (Number(widget.x) || 0) + (Number(widget.w) || 0))
    extent.maxY = Math.max(extent.maxY, (Number(widget.y) || 0) + (Number(widget.h) || 0))
  })
  visibleConnections.value.forEach(conn => {
    renderConnectionPoints(conn).forEach(point => extendEditorExtent(extent, point))
  })
  if (!Number.isFinite(extent.minX) || !Number.isFinite(extent.minY) ||
      !Number.isFinite(extent.maxX) || !Number.isFinite(extent.maxY)) {
    return { minX: 0, minY: 0, width: 800, height: 500, margin: 0 }
  }

  const margin = 24
  return {
    minX: extent.minX,
    minY: extent.minY,
    width: Math.max(320, extent.maxX - extent.minX + margin * 2),
    height: Math.max(240, extent.maxY - extent.minY + margin * 2),
    margin
  }
})

const applicationCanvasStyle = computed(() => ({
  width: applicationBounds.value.width + 'px',
  height: applicationBounds.value.height + 'px'
}))
const applicationZoomPercent = computed(() => Math.round(applicationZoom.value * 100))
const applicationScaledSize = computed(() => ({
  width: Math.ceil(applicationBounds.value.width * applicationZoom.value),
  height: Math.ceil(applicationBounds.value.height * applicationZoom.value)
}))
const applicationStageSize = computed(() => ({
  width: Math.max(applicationScaledSize.value.width, applicationCanvasSize.value.width),
  height: Math.max(applicationScaledSize.value.height, applicationCanvasSize.value.height)
}))
const applicationStageStyle = computed(() => ({
  width: `${applicationStageSize.value.width}px`,
  height: `${applicationStageSize.value.height}px`
}))
const applicationContentStyle = computed(() => {
  const offsetX = Math.max(0, (applicationStageSize.value.width - applicationScaledSize.value.width) / 2)
  const offsetY = Math.max(0, (applicationStageSize.value.height - applicationScaledSize.value.height) / 2)
  return {
    ...applicationCanvasStyle.value,
    transform: `translate(${offsetX}px, ${offsetY}px) scale(${applicationZoom.value})`,
    transformOrigin: '0 0'
  }
})

const applicationWidgets = computed(() => {
  const b = applicationBounds.value
  return store.widgets.map(w => ({
    ...w,
    x: w.x - b.minX + b.margin,
    y: w.y - b.minY + b.margin
  }))
})

function layerWeight(widget) {
  return widget?.type === 'frameBox' ? 0 : 1
}

const orderedEditorWidgets = computed(() =>
  store.widgets
    .map((widget, index) => ({ widget, index }))
    .sort((a, b) => layerWeight(a.widget) - layerWeight(b.widget) || a.index - b.index)
    .map(item => item.widget)
)

const orderedApplicationWidgets = computed(() =>
  applicationWidgets.value
    .map((widget, index) => ({ widget, index }))
    .sort((a, b) => layerWeight(a.widget) - layerWeight(b.widget) || a.index - b.index)
    .map(item => item.widget)
)

const frameBoxHitWidgets = computed(() =>
  store.widgets.filter(widget =>
    widget.type === 'frameBox' &&
    selectedId.value !== widget.id &&
    !selectedWidgetSet.value.has(widget.id)
  )
)

function frameBoxHitProxyStyle(widget) {
  return {
    left: `${editorCanvasX(widget.x)}px`,
    top: `${editorCanvasY(widget.y)}px`,
    width: `${Math.max(1, Number(widget.w) || 1)}px`,
    height: `${Math.max(1, Number(widget.h) || 1)}px`
  }
}

function selectFrameBoxFromProxy(id) {
  selectWidget(id)
}

const applicationWidgetMap = computed(() => {
  const map = {}
  applicationWidgets.value.forEach(w => { map[w.id] = w })
  return map
})

const editorContentBounds = computed(() => {
  if (store.widgets.length === 0) {
    return { minX: 0, minY: 0, maxX: 0, maxY: 0, centerX: 0, centerY: 0 }
  }
  const minX = Math.min(...store.widgets.map(w => Number(w.x) || 0))
  const minY = Math.min(...store.widgets.map(w => Number(w.y) || 0))
  const maxX = Math.max(...store.widgets.map(w => (Number(w.x) || 0) + (Number(w.w) || 0)))
  const maxY = Math.max(...store.widgets.map(w => (Number(w.y) || 0) + (Number(w.h) || 0)))
  return {
    minX,
    minY,
    maxX,
    maxY,
    centerX: minX + (maxX - minX) / 2,
    centerY: minY + (maxY - minY) / 2
  }
})

function extendEditorExtent(extent, point) {
  const x = Number(point?.x)
  const y = Number(point?.y)
  if (!Number.isFinite(x) || !Number.isFinite(y)) return
  extent.minX = Math.min(extent.minX, x)
  extent.minY = Math.min(extent.minY, y)
  extent.maxX = Math.max(extent.maxX, x)
  extent.maxY = Math.max(extent.maxY, y)
}

const editorCanvasBounds = computed(() => {
  const extent = { minX: 0, minY: 0, maxX: 0, maxY: 0 }

  store.widgets.forEach(widget => {
    extent.minX = Math.min(extent.minX, Number(widget.x) || 0)
    extent.minY = Math.min(extent.minY, Number(widget.y) || 0)
    extent.maxX = Math.max(extent.maxX, (Number(widget.x) || 0) + (Number(widget.w) || 0))
    extent.maxY = Math.max(extent.maxY, (Number(widget.y) || 0) + (Number(widget.h) || 0))
  })

  visibleConnections.value.forEach(conn => {
    if (Array.isArray(conn.points)) {
      conn.points.forEach(point => extendEditorExtent(extent, point))
    }
    if (Array.isArray(conn.waypoints)) {
      conn.waypoints.forEach(point => extendEditorExtent(extent, point))
    }
    if (Number.isFinite(Number(conn.bendX))) {
      const bendX = Number(conn.bendX)
      extent.minX = Math.min(extent.minX, bendX)
      extent.maxX = Math.max(extent.maxX, bendX)
    }
    if (Number.isFinite(Number(conn.bendY))) {
      const bendY = Number(conn.bendY)
      extent.minY = Math.min(extent.minY, bendY)
      extent.maxY = Math.max(extent.maxY, bendY)
    }
  })

  if (rubberBand.value?.points?.length) {
    rubberBand.value.points.forEach(point => extendEditorExtent(extent, point))
  }

  const offsetX = Math.max(EDITOR_CANVAS_ORIGIN_X, -extent.minX + EDITOR_CANVAS_PADDING)
  const offsetY = Math.max(EDITOR_CANVAS_ORIGIN_Y, -extent.minY + EDITOR_CANVAS_PADDING)

  return {
    offsetX,
    offsetY,
    width: Math.max(
      EDITOR_CANVAS_MIN_WIDTH,
      Math.ceil(extent.maxX + offsetX + EDITOR_CANVAS_PADDING)
    ),
    height: Math.max(
      EDITOR_CANVAS_MIN_HEIGHT,
      Math.ceil(extent.maxY + offsetY + EDITOR_CANVAS_PADDING)
    )
  }
})

const editorCanvasOffset = computed(() => ({
  x: editorCanvasBounds.value.offsetX,
  y: editorCanvasBounds.value.offsetY
}))

const editorCanvasSizeStyle = computed(() => ({
  width: `${editorCanvasBounds.value.width}px`,
  height: `${editorCanvasBounds.value.height}px`
}))

const selectedWidgetSet = computed(() => new Set(selectedWidgetIds.value))

function normalizeRectFromPoints(a, b) {
  const x1 = Number(a?.x) || 0
  const y1 = Number(a?.y) || 0
  const x2 = Number(b?.x) || 0
  const y2 = Number(b?.y) || 0
  return {
    minX: Math.min(x1, x2),
    minY: Math.min(y1, y2),
    maxX: Math.max(x1, x2),
    maxY: Math.max(y1, y2),
    width: Math.abs(x2 - x1),
    height: Math.abs(y2 - y1)
  }
}

function widgetBounds(widget) {
  return {
    minX: Number(widget.x) || 0,
    minY: Number(widget.y) || 0,
    maxX: (Number(widget.x) || 0) + (Number(widget.w) || 0),
    maxY: (Number(widget.y) || 0) + (Number(widget.h) || 0)
  }
}

function rectIntersects(a, b) {
  return a.minX <= b.maxX && a.maxX >= b.minX && a.minY <= b.maxY && a.maxY >= b.minY
}

const areaSelectionStyle = computed(() => {
  if (!areaSelection.value) return {}
  const rect = normalizeRectFromPoints(areaSelection.value.start, areaSelection.value.current)
  return {
    left: `${editorCanvasX(rect.minX)}px`,
    top: `${editorCanvasY(rect.minY)}px`,
    width: `${rect.width}px`,
    height: `${rect.height}px`
  }
})

const selectedWidgetsBounds = computed(() => {
  const selected = store.widgets.filter(widget => selectedWidgetSet.value.has(widget.id))
  if (!selected.length) return null
  return selected
    .map(widgetBounds)
    .reduce((acc, item) => ({
      minX: Math.min(acc.minX, item.minX),
      minY: Math.min(acc.minY, item.minY),
      maxX: Math.max(acc.maxX, item.maxX),
      maxY: Math.max(acc.maxY, item.maxY)
    }), { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity })
})

const areaSelectionToolbarStyle = computed(() => {
  const bounds = selectedWidgetsBounds.value
  if (!bounds) return {}
  return {
    left: `${editorCanvasX(bounds.minX)}px`,
    top: `${Math.max(8, editorCanvasY(bounds.minY) - 42)}px`
  }
})

// Expanding the infinite canvas can move its internal origin. Compensate the
// viewport during a widget transform so the dragged widget stays under the pointer.
watch(
  () => [editorCanvasBounds.value.offsetX, editorCanvasBounds.value.offsetY],
  ([nextX, nextY], [previousX, previousY]) => {
    if (!activeWidgetTransformId.value) return
    const deltaX = nextX - previousX
    const deltaY = nextY - previousY
    if (deltaX) panX.value -= deltaX * zoom.value
    if (deltaY) panY.value -= deltaY * zoom.value
  },
  { flush: 'sync' }
)

function focusCanvasContent() {
  const area = canvasAreaRef.value
  if (!area || store.widgets.length === 0) return
  const rect = area.getBoundingClientRect()
  const centerX = editorCanvasX(editorContentBounds.value.centerX)
  const centerY = editorCanvasY(editorContentBounds.value.centerY)
  panX.value = rect.width / 2 - centerX * zoom.value
  panY.value = rect.height / 2 - centerY * zoom.value
}

function startCanvasPan(e) {
  if (e.button !== 1) return
  e.preventDefault()
  isPanning.value = true
  panStart.value = { x: e.clientX, y: e.clientY }
  panOrigin.value = { x: panX.value, y: panY.value }
  canvasAreaRef.value.classList.add('panning')
  document.addEventListener('mousemove', onCanvasPanMove)
  document.addEventListener('mouseup', stopCanvasPan)
}

function onCanvasPanMove(e) {
  if (!isPanning.value) return
  pendingPan = {
    x: panOrigin.value.x + (e.clientX - panStart.value.x),
    y: panOrigin.value.y + (e.clientY - panStart.value.y)
  }
  if (!panFrame) {
    panFrame = requestAnimationFrame(() => {
      panFrame = 0
      if (!pendingPan) return
      panX.value = pendingPan.x
      panY.value = pendingPan.y
      pendingPan = null
    })
  }
}

function stopCanvasPan() {
  isPanning.value = false
  if (panFrame) {
    cancelAnimationFrame(panFrame)
    panFrame = 0
  }
  if (pendingPan) {
    panX.value = pendingPan.x
    panY.value = pendingPan.y
    pendingPan = null
  }
  if (canvasAreaRef.value) canvasAreaRef.value.classList.remove('panning')
  document.removeEventListener('mousemove', onCanvasPanMove)
  document.removeEventListener('mouseup', stopCanvasPan)
}

function handleWheel(e) {
  pendingWheel.deltaY += e.deltaY
  pendingWheel.clientX = e.clientX
  pendingWheel.clientY = e.clientY
  isZooming.value = true
  if (wheelIdleTimer) clearTimeout(wheelIdleTimer)
  wheelIdleTimer = setTimeout(() => {
    isZooming.value = false
    requestAnimationFrame(() => {
      zoomRefreshTick.value += 1
    })
    wheelIdleTimer = null
  }, 90)
  if (!wheelFrame) {
    wheelFrame = requestAnimationFrame(applyWheelZoom)
  }
}

function applyWheelZoom() {
  wheelFrame = 0
  const totalDelta = pendingWheel.deltaY
  pendingWheel.deltaY = 0
  const host = canvasAreaRef.value
  if (!host || !Number.isFinite(totalDelta) || Math.abs(totalDelta) < 0.001) return

  const oldZoom = zoom.value
  const zoomFactor = Math.exp(-totalDelta * 0.0011)
  const newZoom = Math.max(0.2, Math.min(5, oldZoom * zoomFactor))
  if (Math.abs(newZoom - oldZoom) < 0.0001) return

  const rect = host.getBoundingClientRect()
  const mx = pendingWheel.clientX - rect.left
  const my = pendingWheel.clientY - rect.top
  const scale = newZoom / oldZoom
  panX.value = mx - (mx - panX.value) * scale
  panY.value = my - (my - panY.value) * scale
  zoom.value = Math.round(newZoom * 10000) / 10000
}

function typeLabel(type) {
  const map = {
    trendChart:'趋势曲线',
    trendChartLite:'简洁趋势曲线',
    digitalDisplay:'数值显示',
    valueColumn:'液柱显示',
    gauge:'仪表盘',
    button:'控制按钮',
    winccToggleButton:'状态切换按钮',
    stepperControl:'加减调节',
    indicator:'状态指示',
    statusCircle:'状态圆点',
    label:'文本标签',
    scadaSvg:'导入 SVG/图片',
    customShape:'自定义组件',
    processPump:'水泵/气泵',
    processDosingMachine:'加药机',
    processJunction:'连接节点',
    processPool:'水池',
    processDam:'尾矿库',
    processTank:'水罐',
    processMixer:'浓密机',
    processValve:'横向阀门',
    processValveVertical:'立式阀门',
    processPipeEnd:'管道端口',
    processValueTag:'数据标牌',
    processCalcinerFurnace:'焙烧炉',
    processCycloneSeparator:'旋风分离器',
    processPreheaterTower:'预热/冷却器',
    processBagFilter:'布袋除尘器',
    processFanBlower:'引风机',
    processWinccFan:'风机',
    processBurner:'燃烧器',
    processAluminaSilo:'氧化铝料仓',
    processScrewConveyor:'螺旋输送机',
    processBucketElevator:'斗式提升机',
    processGasValveSkid:'燃气阀组',
    processStatusMatrix:'状态灯盘',
    processHeatExchanger:'换热器',
    processKilnTube:'炉筒子',
    processStack:'烟囱/排气筒',
    processRotaryAirlock:'星型下料阀',
    processDamper:'风门挡板',
    processSprayLance:'喷枪',
    processMaterialChute:'斜溜槽',
    processDoubleFlapValve:'双层翻板阀'
  }
  return map[type] || type
}

function cloneConfig(config) {
  return JSON.parse(JSON.stringify(config || {}))
}

function ensureConfigDraft(widget, force = false) {
  if (!widget) return {}
  if (force || !configDrafts[widget.id]) {
    configDrafts[widget.id] = cloneConfig(widget.config)
    if (!Number.isFinite(Number(configDrafts[widget.id].rotation))) configDrafts[widget.id].rotation = 0
    if (!Number.isFinite(Number(configDrafts[widget.id].rotationStep))) configDrafts[widget.id].rotationStep = 90
  }
  return configDrafts[widget.id]
}

function normalizeExclusivePeerIds(value) {
  if (!Array.isArray(value)) return []
  return [...new Set(value
    .map(peerId => String(peerId || '').trim())
    .filter(Boolean))]
}

function updateToggleWidgetConfig(widget, nextConfig, historyKey) {
  if (!widget) return
  store.updateWidget(widget.id, { config: cloneConfig(nextConfig) }, historyKey)
}

function setToggleExclusiveEnabled(enabled) {
  if (!configWidget.value || configWidget.value.type !== 'winccToggleButton') return
  const owner = configWidget.value
  const ownerId = String(owner.id)
  const draft = activeConfig.value
  if (enabled) {
    draft.exclusiveEnabled = true
    draft.exclusivePeerIds = normalizeExclusivePeerIds(draft.exclusivePeerIds)
    applyConfig()
    return
  }

  // Removing a button from a group must remove both directions of every
  // relation, otherwise an old one-way link would still reset its color.
  const linkedIds = new Set(getConnectedToggleButtonIds(ownerId))
  linkedIds.delete(ownerId)
  draft.exclusiveEnabled = false
  draft.exclusivePeerIds = []
  draft.exclusiveLabel = ''
  applyConfig()
  linkedIds.forEach((peerId) => {
    const peer = store.widgets.find(widget => String(widget.id) === peerId)
    if (!peer?.config) return
    const peerConfig = cloneConfig(peer.config)
    peerConfig.exclusivePeerIds = normalizeExclusivePeerIds(peerConfig.exclusivePeerIds)
      .filter(id => id !== ownerId)
    if (!peerConfig.exclusivePeerIds.length) {
      peerConfig.exclusiveEnabled = false
      peerConfig.exclusiveLabel = ''
    }
    updateToggleWidgetConfig(peer, peerConfig, `toggle-exclusive-remove:${ownerId}`)
  })
}

function setToggleExclusivePeer(peerId, selected) {
  if (!configWidget.value || configWidget.value.type !== 'winccToggleButton') return
  const owner = configWidget.value
  const ownerId = String(owner.id)
  const normalizedPeerId = String(peerId)
  const peer = store.widgets.find(widget => String(widget.id) === normalizedPeerId)
  if (!peer || peer.type !== 'winccToggleButton' || !peer.config) return

  const draft = activeConfig.value
  const ownPeerIds = new Set(normalizeExclusivePeerIds(draft.exclusivePeerIds))
  const peerConfig = cloneConfig(peer.config)
  const peerPeerIds = new Set(normalizeExclusivePeerIds(peerConfig.exclusivePeerIds))
  if (selected) {
    ownPeerIds.add(normalizedPeerId)
    peerPeerIds.add(ownerId)
    draft.exclusiveEnabled = true
    peerConfig.exclusiveEnabled = true
  } else {
    ownPeerIds.delete(normalizedPeerId)
    peerPeerIds.delete(ownerId)
    if (!peerPeerIds.size) {
      peerConfig.exclusiveEnabled = false
      peerConfig.exclusiveLabel = ''
    }
  }
  draft.exclusivePeerIds = [...ownPeerIds]
  if (!ownPeerIds.size) {
    draft.exclusiveEnabled = false
    draft.exclusiveLabel = ''
  }
  peerConfig.exclusivePeerIds = [...peerPeerIds]
  applyConfig()
  updateToggleWidgetConfig(peer, peerConfig, `toggle-exclusive-link:${ownerId}`)
}

function getConnectedToggleButtonIds(widgetId) {
  const startId = String(widgetId)
  const toggleButtons = store.widgets.filter(widget => widget?.type === 'winccToggleButton')
  const knownIds = new Set(toggleButtons.map(widget => String(widget.id)))
  const visited = new Set([startId])
  const queue = [startId]
  while (queue.length) {
    const currentId = queue.shift()
    const current = toggleButtons.find(widget => String(widget.id) === currentId)
    const currentPeers = current?.config?.exclusiveEnabled
      ? new Set(normalizeExclusivePeerIds(current.config.exclusivePeerIds))
      : new Set()
    toggleButtons.forEach((candidate) => {
      const candidateId = String(candidate.id)
      if (visited.has(candidateId) || candidateId === currentId) return
      const candidatePeers = candidate.config?.exclusiveEnabled
        ? new Set(normalizeExclusivePeerIds(candidate.config.exclusivePeerIds))
        : new Set()
      if (currentPeers.has(candidateId) || candidatePeers.has(currentId)) {
        if (!knownIds.has(candidateId)) return
        visited.add(candidateId)
        queue.push(candidateId)
      }
    })
  }
  return [...visited]
}

function selectWidget(id) {
  selectedId.value = id
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
}

function handleWidgetSelect(id) {
  openConfig(id)
}

function clearSelection() {
  selectedId.value = null
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
  configWidgetId.value = null
  if (connectionMode.value) connectionDraftFrom.value = null
}

function handleCanvasBlankClick() {
  if (suppressNextCanvasClick) {
    suppressNextCanvasClick = false
    return
  }
  clearSelection()
}

function selectWidgetsByIds(ids = []) {
  const validIds = ids.filter(id => store.widgets.some(widget => widget.id === id))
  selectedWidgetIds.value = validIds
  selectedConnectionId.value = null
  connectionDraftFrom.value = null
  configWidgetId.value = null
  selectedId.value = validIds.length === 1 ? validIds[0] : null
}

function widgetsInsideRect(rect) {
  return store.widgets
    .filter(widget => widget.type !== 'frameBox')
    .filter(widget => rectIntersects(rect, widgetBounds(widget)))
    .map(widget => widget.id)
}

function selectedInternalConnections(ids = selectedWidgetIds.value) {
  const set = new Set(ids)
  return store.connections.filter(connection =>
    connection.from && connection.to && set.has(connection.from) && set.has(connection.to)
  )
}

function selectedWidgetSources(ids = selectedWidgetIds.value) {
  const set = new Set(ids)
  return store.widgets.filter(widget => set.has(widget.id))
}

function startAreaSelection(e) {
  if (applicationMode.value || connectionMode.value || rubberBand.value || e.button !== 0) return
  const target = e.target instanceof Element ? e.target : null
  if (target?.closest?.('.widget-wrapper, .connection-group, .area-selection-toolbar, .platform-context-menu')) return
  e.preventDefault()
  closeContextMenu()
  const start = canvasPointFromEvent(e)
  const startClient = { x: e.clientX, y: e.clientY }
  areaSelection.value = { start, current: start }
  selectedId.value = null
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
  configWidgetId.value = null

  const onMove = (ev) => {
    areaSelection.value = {
      start,
      current: canvasPointFromEvent(ev)
    }
  }

  const onUp = (ev) => {
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
    const end = canvasPointFromEvent(ev)
    const moved = Math.hypot(ev.clientX - startClient.x, ev.clientY - startClient.y)
    areaSelection.value = null
    if (moved < 4) {
      clearSelection()
      return
    }
    const rect = normalizeRectFromPoints(start, end)
    const ids = widgetsInsideRect(rect)
    selectWidgetsByIds(ids)
    suppressNextCanvasClick = true
    window.setTimeout(() => { suppressNextCanvasClick = false }, 0)
    if (ids.length) showConnectionNotice(`已框选 ${ids.length} 个组件`)
  }

  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function copySelectedWidgets() {
  const widgets = selectedWidgetSources()
  if (!widgets.length) return false
  copiedSelection.value = {
    widgets: JSON.parse(JSON.stringify(widgets)),
    connections: JSON.parse(JSON.stringify(selectedInternalConnections())),
    bounds: selectedWidgetsBounds.value ? JSON.parse(JSON.stringify(selectedWidgetsBounds.value)) : null
  }
  copiedWidget.value = null
  pasteSequence = 0
  showConnectionNotice(`已复制 ${widgets.length} 个组件`)
  return true
}

function pasteCopiedSelection(point = null) {
  if (!copiedSelection.value?.widgets?.length) return null
  const source = copiedSelection.value
  pasteSequence += 1
  let offset = { x: 24 * pasteSequence, y: 24 * pasteSequence }
  if (point && source.bounds) {
    const centerX = source.bounds.minX + (source.bounds.maxX - source.bounds.minX) / 2
    const centerY = source.bounds.minY + (source.bounds.maxY - source.bounds.minY) / 2
    offset = {
      x: Math.round(Number(point.x) - centerX),
      y: Math.round(Number(point.y) - centerY)
    }
  }
  const result = store.duplicateWidgetsWithConnections(source.widgets, source.connections, offset)
  const ids = result.widgets.map(widget => widget.id)
  selectWidgetsByIds(ids)
  if (ids.length) showConnectionNotice(`已粘贴 ${ids.length} 个组件`)
  return result
}

function deleteSelectedWidgets() {
  const ids = selectedWidgetIds.value.slice()
  if (!ids.length) return
  const removed = store.removeWidgets(ids)
  selectedWidgetIds.value = []
  selectedId.value = null
  selectedConnectionId.value = null
  configWidgetId.value = null
  connectionDraftFrom.value = null
  if (removed.length) showConnectionNotice(`已删除 ${removed.length} 个组件`)
}

function toggleConnectionMode() {
  cancelWaypointConnection(false)
  connectionMode.value = !connectionMode.value
  connectionDraftFrom.value = null
  selectedConnectionId.value = null
  selectedWidgetIds.value = []
  if (connectionMode.value) {
    selectedId.value = null
    configWidgetId.value = null
  }
}

function portInfoFromElement(el) {
  const anchor = el?.closest?.('.connection-port-anchor')
  if (!anchor) return null
  const widgetId = anchor.dataset.widgetId
  const port = anchor.dataset.portName
  if (!widgetId || !PORT_NAMES.includes(port)) return null
  const anchorX = Number(anchor.dataset.anchorX)
  const anchorY = Number(anchor.dataset.anchorY)
  const customAnchor = Number.isFinite(anchorX) && Number.isFinite(anchorY)
    ? { x: anchorX, y: anchorY, side: port }
    : null
  return { widgetId, port, anchor: customAnchor }
}

function portInfoFromEvent(e) {
  const elements = document.elementsFromPoint(e.clientX, e.clientY)
  for (const el of elements) {
    const info = portInfoFromElement(el)
    if (info) return info
  }
  return null
}

function isCustomConnectableWidget(widget) {
  return Boolean(widget) &&
    (String(widget.type || '').startsWith('process') || widget.type === 'customShape') &&
    widget.type !== 'processValueTag'
}

function customPortInfoForWidget(widgetId, event) {
  const widget = widgetMap.value[widgetId]
  if (!isCustomConnectableWidget(widget)) return null
  const point = canvasPointFromEvent(event)
  const bounds = widgetVisualBounds(widget)
  const clampedX = Math.max(bounds.x, Math.min(bounds.x + bounds.w, point.x))
  const clampedY = Math.max(bounds.y, Math.min(bounds.y + bounds.h, point.y))
  const candidates = [
    { port: 'left', distance: Math.abs(point.x - bounds.x), x: bounds.x, y: clampedY },
    { port: 'right', distance: Math.abs(point.x - (bounds.x + bounds.w)), x: bounds.x + bounds.w, y: clampedY },
    { port: 'top', distance: Math.abs(point.y - bounds.y), x: clampedX, y: bounds.y },
    { port: 'bottom', distance: Math.abs(point.y - (bounds.y + bounds.h)), x: clampedX, y: bounds.y + bounds.h }
  ]
  const nearest = candidates.sort((a, b) => a.distance - b.distance)[0]
  if (!nearest) return null
  const width = Math.max(1, Number(widget.w) || 1)
  const height = Math.max(1, Number(widget.h) || 1)
  return {
    widgetId,
    port: nearest.port,
    anchor: {
      x: Math.max(0, Math.min(1, (nearest.x - (Number(widget.x) || 0)) / width)),
      y: Math.max(0, Math.min(1, (nearest.y - (Number(widget.y) || 0)) / height)),
      side: nearest.port,
      free: true
    }
  }
}

function customPortInfoFromEvent(e) {
  const elements = document.elementsFromPoint(e.clientX, e.clientY)
  for (const element of elements) {
    const wrapper = element?.closest?.('.widget-wrapper')
    const widgetId = wrapper?.dataset?.widgetId
    if (!widgetId) continue
    const info = customPortInfoForWidget(widgetId, e)
    if (info) return info
  }
  return null
}

function isInputPort(port) {
  return port === 'left'
}

function isInputPortOccupied(target) {
  if (target.anchor) return false
  if (!isInputPort(target.port)) return false
  return store.connections.some(conn =>
    conn.to === target.widgetId && conn.toPort === target.port
  )
}

function selfLoopWaypoints(source, target) {
  const widget = widgetMap.value[source.widgetId]
  if (!widget) return null
  const b = widgetVisualBounds(widget)
  const start = connectionWidgetAnchor(widget, widget, source.port, source.anchor)
  const end = connectionWidgetAnchor(widget, widget, target.port, target.anchor)
  const margin = Math.max(48, Math.min(90, Math.max(widget.w, widget.h) * 0.35))
  const topY = b.y - margin
  const bottomY = b.y + b.h + margin
  const leftX = b.x - margin
  const rightX = b.x + b.w + margin

  if (source.port === 'right' || target.port === 'right') {
    return [
      { x: rightX, y: start.y },
      { x: rightX, y: topY },
      { x: end.x, y: topY }
    ]
  }
  if (source.port === 'left' || target.port === 'left') {
    return [
      { x: leftX, y: start.y },
      { x: leftX, y: topY },
      { x: end.x, y: topY }
    ]
  }
  if (source.port === 'bottom' || target.port === 'bottom') {
    return [
      { x: start.x, y: bottomY },
      { x: rightX, y: bottomY },
      { x: rightX, y: end.y }
    ]
  }
  return [
    { x: start.x, y: topY },
    { x: rightX, y: topY },
    { x: rightX, y: end.y }
  ]
}

function completePortConnection(source, target, event, manualWaypoints = []) {
  if (!source || !target) return
  const hasFreeEndpoint = Boolean(source.anchor?.free || target.anchor?.free)
  if (source.widgetId === target.widgetId && !event?.ctrlKey && !hasFreeEndpoint) {
    showConnectionNotice('同一组件端口默认不允许自环连接；按住 Ctrl 可强制创建。')
    return
  }
  if (isInputPortOccupied(target)) {
    showConnectionNotice('该输入端口已被占用，默认不允许重复连接。')
    return
  }
  const useStraightRoute = Boolean(event?.altKey && !manualWaypoints.length)
  const options = {
    fromPort: source.port,
    toPort: target.port,
    fromAnchor: source.anchor || null,
    toAnchor: target.anchor || null,
    allowSelf: Boolean(event?.ctrlKey) || hasFreeEndpoint,
    routeMode: useStraightRoute ? 'straight' : null,
    bendX: null,
    bendY: null
  }
  if (manualWaypoints.length) {
    options.waypoints = manualWaypoints
    options.manualRoute = true
  } else if (useStraightRoute) {
    options.manualRoute = true
  } else if (source.widgetId === target.widgetId) {
    options.waypoints = selfLoopWaypoints(source, target)
  }
  const conn = store.addConnection(source.widgetId, target.widgetId, options)
  if (!conn) {
    showConnectionNotice('连线未创建，请检查起点和终点端口。')
    return
  }
  selectedConnectionId.value = manualWaypoints.length ? null : conn.id
  selectedId.value = null
}

function pointTargetWidget(point) {
  return {
    id: '__cursor__',
    type: '__cursor__',
    x: Number(point.x) || 0,
    y: Number(point.y) || 0,
    w: 0,
    h: 0
  }
}

function connectionPointInfo(connection, event) {
  if (!connection) return null
  const points = renderConnectionPoints(connection)
  if (points.length < 2) return null
  const cursor = canvasPointFromEvent(event)
  const segmentIndex = nearestSegmentIndex(points, cursor)
  if (segmentIndex < 0) return null
  return {
    kind: 'connection',
    connectionId: connection.id,
    segmentIndex,
    point: roundPoint(pointOnSegmentFromCursor(cursor, points[segmentIndex], points[segmentIndex + 1]))
  }
}

function connectionInfoFromEvent(event, excludeConnectionId = null) {
  const elements = document.elementsFromPoint(event.clientX, event.clientY)
  for (const element of elements) {
    const group = element?.closest?.('.connection-group')
    const connectionId = group?.dataset?.connectionId
    if (!connectionId || connectionId === excludeConnectionId) continue
    const connection = store.connections.find(item => item.id === connectionId)
    const info = connectionPointInfo(connection, event)
    if (info) return info
  }
  return null
}

function connectionTargetFromEvent(event, source = null) {
  const fixedPort = portInfoFromEvent(event)
  if (fixedPort) return { kind: 'widget', ...fixedPort }
  const allowFreeTarget = Boolean(event.ctrlKey || source?.anchor?.free || source?.kind === 'connection')
  if (!allowFreeTarget) return null
  const customPort = customPortInfoFromEvent(event)
  if (customPort) return { kind: 'widget', ...customPort }
  return connectionInfoFromEvent(event, source?.connectionId || null)
}

function sourcePointForPreview(source, targetPoint) {
  if (source.kind === 'connection') return source.point
  const sourceWidget = widgetMap.value[source.widgetId]
  if (!sourceWidget) return targetPoint
  const targetWidget = source.targetWidget || pointTargetWidget(targetPoint)
  return connectionWidgetAnchor(sourceWidget, targetWidget, source.port, source.anchor)
}

function targetPointForPreview(target, sourcePoint) {
  if (!target) return null
  if (target.kind === 'connection') return target.point
  const targetWidget = widgetMap.value[target.widgetId]
  if (!targetWidget) return null
  return connectionWidgetAnchor(targetWidget, pointTargetWidget(sourcePoint), target.port, target.anchor)
}

function buildRubberBandPreview(source, cursor, event) {
  const target = connectionTargetFromEvent(event, source)
  const start = sourcePointForPreview(source, cursor)
  const resolvedEnd = targetPointForPreview(target, start)
  const snap = resolvedEnd
    ? { point: resolvedEnd, snapped: true, guide: null }
    : smartOrthogonalSnap(cursor, [start], event)
  const end = resolvedEnd || (snap.snapped ? snap.point : cursor)
  const useStraightPreview = Boolean(event?.altKey)
  const route = useStraightPreview
    ? routeForMode(start, end, 'straight', {
        fromPort: source.kind === 'widget' ? source.port : null,
        toPort: target?.kind === 'widget' ? target.port : null
      })
    : (source.kind === 'widget'
      ? chooseDirectionAwareRoute(start, end, {
        from: source.widgetId,
        to: target?.kind === 'widget' ? target.widgetId : null,
        fromPort: source.port,
        toPort: target?.kind === 'widget' ? target.port : null
      })
      : routeForMode(start, end, routeModeForPoints(start, end), {}))

  return {
    start,
    end,
    points: route.points || [start, end],
    snapped: Boolean(target || snap.snapped),
    guide: target ? null : snap.guide,
    target,
    manual: false
  }
}

function orthogonalWaypoint(lastPoint, rawPoint, source = null, firstPoint = false) {
  const last = roundPoint(lastPoint)
  const raw = roundPoint(rawPoint)
  const direction = firstPoint && source?.kind === 'widget'
    ? portDirection(source.port)
    : null

  if (direction?.x) {
    const x = direction.x > 0 ? Math.max(last.x, raw.x) : Math.min(last.x, raw.x)
    return { x, y: last.y }
  }
  if (direction?.y) {
    const y = direction.y > 0 ? Math.max(last.y, raw.y) : Math.min(last.y, raw.y)
    return { x: last.x, y }
  }

  return Math.abs(raw.x - last.x) >= Math.abs(raw.y - last.y)
    ? { x: raw.x, y: last.y }
    : { x: last.x, y: raw.y }
}

function waypointPreviewPoints(session, target, end, previewPoint = null) {
  const points = [session.start]
  if (session.source.kind === 'widget') points.push(session.startExit)
  points.push(...session.waypoints)
  if (previewPoint) points.push(previewPoint)
  if (target?.kind === 'widget') points.push(offsetPointByPort(end, target.port))
  points.push(end)
  return orthogonalizeConnectionPoints(points)
}

function splitConnectionAtInfo(info) {
  const connection = store.connections.find(item => item.id === info?.connectionId)
  if (!connection) return null
  const points = renderConnectionPoints(connection)
  if (points.length < 2) return null
  const index = Math.max(0, Math.min(points.length - 2, Number(info.segmentIndex) || 0))
  const point = pointOnSegmentFromCursor(info.point, points[index], points[index + 1])
  const before = compactConnectionPoints([...points.slice(0, index + 1), point])
  const after = compactConnectionPoints([point, ...points.slice(index + 1)])
  const previous = before.length >= 2 ? before[before.length - 2] : null
  const next = after.length >= 2 ? after[1] : null
  return store.splitConnectionWithJunction(
    connection.id,
    roundPoint(point),
    before.slice(1, -1),
    after.slice(1, -1),
    {
      incomingJunctionAnchor: junctionAnchorFacing(point, previous),
      outgoingJunctionAnchor: junctionAnchorFacing(point, next)
    }
  )
}

function junctionCenter(junction) {
  return {
    x: (Number(junction?.x) || 0) + (Number(junction?.w) || 0) / 2,
    y: (Number(junction?.y) || 0) + (Number(junction?.h) || 0) / 2
  }
}

function junctionAnchorFacing(centerPoint, neighborPoint) {
  const center = roundPoint(centerPoint || { x: 0, y: 0 })
  const neighbor = neighborPoint ? roundPoint(neighborPoint) : null
  if (!neighbor) return { x: 0.5, y: 0.5, side: 'right' }
  const dx = neighbor.x - center.x
  const dy = neighbor.y - center.y
  if (Math.abs(dx) >= Math.abs(dy)) {
    return dx < 0
      ? { x: 0, y: 0.5, side: 'left' }
      : { x: 1, y: 0.5, side: 'right' }
  }
  return dy < 0
    ? { x: 0.5, y: 0, side: 'top' }
    : { x: 0.5, y: 1, side: 'bottom' }
}

function branchJunctionAnchor(split, neighborPoint) {
  return junctionAnchorFacing(junctionCenter(split?.junction), neighborPoint)
}

function completeFlexibleConnection(source, target, event, manualWaypoints = []) {
  if (source.kind === 'widget' && target.kind === 'widget') {
    completePortConnection(source, target, event, manualWaypoints)
    return
  }
  if (source.kind === 'connection' && target.kind === 'connection') {
    showConnectionNotice('暂不允许连接线直接连接另一条连接线，请先连接到组件或已有分支点。')
    return
  }

  const lineInfo = source.kind === 'connection' ? source : target
  const split = splitConnectionAtInfo(lineInfo)
  if (!split) {
    showConnectionNotice('无法在该位置创建分支节点。')
    return
  }
  const style = {
    color: split.original.color,
    width: split.original.width,
    kind: split.original.kind,
    skipHistory: true
  }
  const center = junctionCenter(split.junction)
  const branchNeighbor = manualWaypoints.length
    ? (source.kind === 'connection' ? manualWaypoints[0] : manualWaypoints[manualWaypoints.length - 1])
    : (source.kind === 'connection'
        ? targetPointForPreview(target, center)
        : sourcePointForPreview(source, center))
  const branch = source.kind === 'connection'
    ? store.addConnection(split.junction.id, target.widgetId, {
      ...style,
      fromAnchor: branchJunctionAnchor(split, branchNeighbor),
      toPort: target.port,
      toAnchor: target.anchor || null,
      waypoints: manualWaypoints.length ? manualWaypoints : null,
      manualRoute: manualWaypoints.length > 0 || Boolean(event?.altKey),
      routeMode: !manualWaypoints.length && event?.altKey ? 'straight' : null
      })
    : store.addConnection(source.widgetId, split.junction.id, {
      ...style,
      fromPort: source.port,
      fromAnchor: source.anchor || null,
      toAnchor: branchJunctionAnchor(split, branchNeighbor),
      waypoints: manualWaypoints.length ? manualWaypoints : null,
      manualRoute: manualWaypoints.length > 0 || Boolean(event?.altKey),
      routeMode: !manualWaypoints.length && event?.altKey ? 'straight' : null
      })
  if (!branch) {
    showConnectionNotice('分支连线创建失败。')
    return
  }
  selectedConnectionId.value = manualWaypoints.length ? null : branch.id
  selectedId.value = null
  showConnectionNotice('已创建连线分支节点。')
}

let waypointConnectionSession = null

function buildWaypointConnectionPreview(session, event) {
  const cursor = canvasPointFromEvent(event)
  const target = connectionTargetFromEvent(event, session.source)
  const resolvedEnd = targetPointForPreview(target, session.start)
  const last = session.waypoints[session.waypoints.length - 1] || session.startExit
  const previewPoint = resolvedEnd
    ? null
    : orthogonalWaypoint(last, cursor, session.source, session.waypoints.length === 0)
  const end = resolvedEnd || previewPoint
  const horizontal = !resolvedEnd && Math.abs(end.y - last.y) <= 0.001
  const vertical = !resolvedEnd && Math.abs(end.x - last.x) <= 0.001

  return {
    start: session.start,
    end,
    points: waypointPreviewPoints(session, target, end, previewPoint),
    fixedPoints: session.waypoints,
    snapped: Boolean(target),
    guide: horizontal
      ? { orientation: 'horizontal', value: last.y }
      : (vertical ? { orientation: 'vertical', value: last.x } : null),
    target,
    manual: true
  }
}

function refreshWaypointConnectionPreview(event) {
  if (!waypointConnectionSession) return
  waypointConnectionSession.lastPointer = {
    clientX: Number(event.clientX) || 0,
    clientY: Number(event.clientY) || 0,
    ctrlKey: Boolean(event.ctrlKey)
  }
  const preview = buildWaypointConnectionPreview(waypointConnectionSession, event)
  rubberBand.value = preview
  applyAlignmentFeedback(
    { snapped: Boolean(preview.guide), guide: preview.guide },
    { type: 'rubber' }
  )
}

function removeWaypointConnectionListeners() {
  document.removeEventListener('mousemove', handleWaypointConnectionMove)
  document.removeEventListener('mousedown', handleWaypointConnectionClick, true)
  document.removeEventListener('keydown', handleWaypointConnectionKeyDown, true)
}

function cancelWaypointConnection(showMessage = true) {
  if (!waypointConnectionSession) return
  removeWaypointConnectionListeners()
  waypointConnectionSession = null
  rubberBand.value = null
  connectionDraftFrom.value = null
  clearAlignmentFeedback()
  if (showMessage) showConnectionNotice('已取消自定义路径连线。')
}

function handleWaypointConnectionMove(event) {
  refreshWaypointConnectionPreview(event)
}

function handleWaypointConnectionKeyDown(event) {
  if (!waypointConnectionSession) return
  if (event.key === 'Escape') {
    event.preventDefault()
    event.stopImmediatePropagation()
    cancelWaypointConnection()
    return
  }
  if (event.key === 'Backspace' && waypointConnectionSession.waypoints.length) {
    event.preventDefault()
    event.stopImmediatePropagation()
    waypointConnectionSession.waypoints.pop()
    refreshWaypointConnectionPreview(waypointConnectionSession.lastPointer || { clientX: 0, clientY: 0 })
  }
}

function handleWaypointConnectionClick(event) {
  const session = waypointConnectionSession
  if (!session || event.button !== 0) return
  if (!event.target?.closest?.('.canvas-area')) return

  event.preventDefault()
  event.stopPropagation()
  const target = connectionTargetFromEvent(event, session.source)
  if (target) {
    const manualWaypoints = session.waypoints.map(point => roundPoint(point))
    const source = session.source
    cancelWaypointConnection(false)
    completeFlexibleConnection(source, target, event, manualWaypoints)
    return
  }

  if (event.target?.closest?.('.widget-wrapper, .connection-group')) {
    showConnectionNotice('请点击目标端口完成连线，或点击画布空白处添加路径点。')
    return
  }

  const cursor = canvasPointFromEvent(event)
  const clickTime = Number(event.timeStamp) || Date.now()
  const previousClick = session.lastClick
  if (previousClick && clickTime - previousClick.time < 140 &&
      Math.hypot(event.clientX - previousClick.x, event.clientY - previousClick.y) < 8) {
    return
  }
  const last = session.waypoints[session.waypoints.length - 1] || session.startExit
  const point = orthogonalWaypoint(last, cursor, session.source, session.waypoints.length === 0)
  if (Math.hypot(point.x - last.x, point.y - last.y) < 2) return
  session.lastClick = { time: clickTime, x: event.clientX, y: event.clientY }
  session.waypoints.push(roundPoint(point))
  refreshWaypointConnectionPreview(event)
}

function beginWaypointConnection(source, event) {
  cancelWaypointConnection(false)
  const cursor = canvasPointFromEvent(event)
  const start = sourcePointForPreview(source, cursor)
  waypointConnectionSession = {
    source,
    start: roundPoint(start),
    startExit: roundPoint(source.kind === 'widget' ? offsetPointByPort(start, source.port) : start),
    waypoints: [],
    lastPointer: null,
    lastClick: null
  }
  connectionDraftFrom.value = source.widgetId || null
  selectedId.value = source.widgetId || null
  selectedConnectionId.value = null
  refreshWaypointConnectionPreview(event)
  document.addEventListener('mousemove', handleWaypointConnectionMove)
  document.addEventListener('mousedown', handleWaypointConnectionClick, true)
  document.addEventListener('keydown', handleWaypointConnectionKeyDown, true)
  showConnectionNotice('自定义路径：左键添加水平/垂直路径点，点击目标端口完成，Esc 取消。')
}

function beginConnectionDrag(source, event) {
  if (!event || event.button !== 0 || !source) return
  if (event.shiftKey) {
    beginWaypointConnection(source, event)
    return
  }
  const previewForEvent = (ev) => {
    const cursor = canvasPointFromEvent(ev)
    return buildRubberBandPreview(source, cursor, ev)
  }
  const initialPreview = previewForEvent(event)
  connectionDraftFrom.value = source.widgetId || null
  selectedId.value = source.widgetId || null
  selectedConnectionId.value = null
  rubberBand.value = initialPreview
  applyAlignmentFeedback(
    { snapped: Boolean(initialPreview.guide), guide: initialPreview.guide },
    { type: 'rubber' }
  )

  const onMove = (ev) => {
    const preview = previewForEvent(ev)
    rubberBand.value = preview
    applyAlignmentFeedback(
      { snapped: Boolean(preview.guide), guide: preview.guide },
      { type: 'rubber' }
    )
  }
  const onUp = (ev) => {
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
    const target = connectionTargetFromEvent(ev, source)
    rubberBand.value = null
    connectionDraftFrom.value = null
    clearAlignmentFeedback()
    if (!target) {
      showConnectionNotice('未对准有效组件边缘、端口或连接线，已取消本次连线。')
      return
    }
    completeFlexibleConnection(source, target, ev)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function startPortConnection(payload) {
  const event = payload?.event
  const sourceWidget = widgetMap.value[payload?.widgetId]
  const sourcePort = payload?.port
  if (!event || !sourceWidget || !PORT_NAMES.includes(sourcePort)) return
  beginConnectionDrag({
    kind: 'widget',
    widgetId: payload.widgetId,
    port: sourcePort,
    anchor: payload.anchor || null
  }, event)
}

function startCustomPortConnection(payload) {
  const event = payload?.event
  if (!event?.ctrlKey) return
  const source = customPortInfoForWidget(payload.widgetId, event)
  if (!source) return
  beginConnectionDrag({ kind: 'widget', ...source }, event)
}

function startConnectionFromLine(connection, event) {
  if (!event?.ctrlKey || event.button !== 0) return
  event.preventDefault()
  const source = connectionPointInfo(connection, event)
  if (!source) return
  beginConnectionDrag(source, event)
}

function selectConnection(id) {
  selectedConnectionId.value = id
  selectedId.value = null
  selectedWidgetIds.value = []
  configWidgetId.value = null
  connectionDraftFrom.value = null
}

function removeSelectedConnection() {
  if (!selectedConnectionId.value) return
  store.removeConnection(selectedConnectionId.value)
  selectedConnectionId.value = null
}

function resetSelectedConnectionRoute() {
  if (!selectedConnectionId.value) return
  const conn = store.connections.find(item => item.id === selectedConnectionId.value)
  if (!conn || !conn.from || !conn.to) return
  store.updateConnection(conn.id, {
    waypoints: null,
    points: null,
    manualRoute: false,
    routeMode: null,
    bendX: null,
    bendY: null
  })
}

function updateSelectedConnection(data) {
  if (!selectedConnectionId.value) return
  store.updateConnection(selectedConnectionId.value, data)
}

function updateSelectedConnectionRouteMode(mode) {
  const routeMode = mode || null
  updateSelectedConnection({
    routeMode,
    manualRoute: Boolean(routeMode),
    waypoints: null,
    points: null,
    bendX: null,
    bendY: null
  })
}

function connectionArrowSize(connection) {
  const size = Number(connection?.arrowSize)
  return Math.max(4, Math.min(48, Number.isFinite(size) ? Math.round(size) : 10))
}

function connectionMarkerKey(connection) {
  return String(connection?.id || 'line').replace(/[^a-zA-Z0-9_-]/g, '-')
}

function connectionMarkerId(connection, direction, app = false) {
  return `${app ? 'app-' : ''}connection-arrow-${direction}-${connectionMarkerKey(connection)}`
}

function connectionMarkerStart(connection, app = false) {
  return connection?.startArrow ? `url(#${connectionMarkerId(connection, 'start', app)})` : null
}

function connectionMarkerEnd(connection, app = false) {
  return connection?.endArrow ? `url(#${connectionMarkerId(connection, 'end', app)})` : null
}

function roundPoint(point) {
  return {
    x: Math.round(Number(point.x) || 0),
    y: Math.round(Number(point.y) || 0)
  }
}

function showConnectionNotice(message) {
  connectionNotice.value = message
  if (connectionNoticeTimer) clearTimeout(connectionNoticeTimer)
  connectionNoticeTimer = setTimeout(() => {
    connectionNotice.value = ''
    connectionNoticeTimer = null
  }, 2600)
}

function clearAlignmentFeedback() {
  orthogonalGuide.value = null
  activeAlignmentHandle.value = null
}

function angleSnapCandidate(anchor, point) {
  const dx = point.x - anchor.x
  const dy = point.y - anchor.y
  if (Math.hypot(dx, dy) < 2) return null
  const angle = Math.abs(Math.atan2(dy, dx) * 180 / Math.PI)
  const horizontalDelta = Math.min(angle, Math.abs(180 - angle))
  const verticalDelta = Math.abs(90 - angle)
  if (horizontalDelta <= ORTHOGONAL_SNAP_DEGREES && horizontalDelta <= verticalDelta) {
    return {
      delta: horizontalDelta,
      point: { x: point.x, y: anchor.y },
      guide: { orientation: 'horizontal', value: anchor.y }
    }
  }
  if (verticalDelta <= ORTHOGONAL_SNAP_DEGREES) {
    return {
      delta: verticalDelta,
      point: { x: anchor.x, y: point.y },
      guide: { orientation: 'vertical', value: anchor.x }
    }
  }
  return null
}

function smartOrthogonalSnap(point, anchors = [], event = null) {
  const raw = {
    x: Number(point.x) || 0,
    y: Number(point.y) || 0
  }
  if (event?.shiftKey || event?.altKey) return { point: raw, snapped: false, guide: null }
  const candidates = anchors
    .filter(Boolean)
    .map(anchor => angleSnapCandidate(anchor, raw))
    .filter(Boolean)
    .sort((a, b) => a.delta - b.delta)
  if (!candidates.length) return { point: raw, snapped: false, guide: null }
  const best = candidates[0]
  return { point: best.point, snapped: true, guide: best.guide }
}

function smartWaypointSnap(point, before, after, event = null) {
  if (event?.shiftKey || !before || !after) return null
  const raw = {
    x: Number(point.x) || 0,
    y: Number(point.y) || 0
  }
  const beforePoint = { x: Number(before.x), y: Number(before.y) }
  const afterPoint = { x: Number(after.x), y: Number(after.y) }
  if (!Number.isFinite(beforePoint.x) || !Number.isFinite(beforePoint.y) ||
      !Number.isFinite(afterPoint.x) || !Number.isFinite(afterPoint.y)) {
    return null
  }

  const horizontalY = (beforePoint.y + afterPoint.y) / 2
  if (Math.abs(beforePoint.y - afterPoint.y) <= PORT_ALIGNMENT_SNAP &&
      Math.abs(raw.y - horizontalY) <= PORT_ALIGNMENT_SNAP) {
    return {
      point: { x: raw.x, y: horizontalY },
      snapped: true,
      guide: { orientation: 'horizontal', value: horizontalY }
    }
  }

  const verticalX = (beforePoint.x + afterPoint.x) / 2
  if (Math.abs(beforePoint.x - afterPoint.x) <= PORT_ALIGNMENT_SNAP &&
      Math.abs(raw.x - verticalX) <= PORT_ALIGNMENT_SNAP) {
    return {
      point: { x: verticalX, y: raw.y },
      snapped: true,
      guide: { orientation: 'vertical', value: verticalX }
    }
  }

  return null
}

function applyAlignmentFeedback(snap, handle = null) {
  orthogonalGuide.value = snap.snapped ? snap.guide : null
  activeAlignmentHandle.value = snap.snapped ? handle : null
}

function widgetVisualBounds(widget) {
  return vectorBounds(widget)
}

function widgetVisualCenter(widget) {
  return vectorCenter(widget)
}

function widgetPort(widget, portName, target = null) {
  return vectorPort(widget, portName, target)
}

function widgetAnchor(widget, target, portName = null) {
  if (isHiddenJunctionWidget(widget)) return widgetCenterPoint(widget)
  return vectorAnchor(widget, target, portName)
}

function widgetCenterPoint(widget) {
  return {
    x: (Number(widget?.x) || 0) + (Number(widget?.w) || 0) / 2,
    y: (Number(widget?.y) || 0) + (Number(widget?.h) || 0) / 2
  }
}

function widgetRotationDegrees(widget) {
  const value = Number(widget?.config?.rotation)
  return Number.isFinite(value) ? value : 0
}

function rotatePointForWidget(widget, point) {
  const angle = widgetRotationDegrees(widget) * Math.PI / 180
  if (!angle || !point) return point
  const center = widgetCenterPoint(widget)
  const dx = Number(point.x) - center.x
  const dy = Number(point.y) - center.y
  const cos = Math.cos(angle)
  const sin = Math.sin(angle)
  return {
    x: center.x + dx * cos - dy * sin,
    y: center.y + dx * sin + dy * cos
  }
}

function isHiddenJunctionWidget(widget) {
  return widget?.type === 'processJunction' && Boolean(widget.config?.hideNode)
}

function customWidgetAnchor(widget, anchor) {
  if (!widget || !anchor) return null
  if (isHiddenJunctionWidget(widget)) return widgetCenterPoint(widget)
  const x = Number(anchor.x)
  const y = Number(anchor.y)
  if (!Number.isFinite(x) || !Number.isFinite(y)) return null
  return rotatePointForWidget(widget, {
    x: Number(widget.x) + Number(widget.w) * Math.max(0, Math.min(1, x)),
    y: Number(widget.y) + Number(widget.h) * Math.max(0, Math.min(1, y))
  })
}

function connectionWidgetAnchor(widget, target, portName = null, anchor = null) {
  if (isHiddenJunctionWidget(widget)) return widgetCenterPoint(widget)
  const customAnchor = customWidgetAnchor(widget, anchor)
  if (customAnchor) return customAnchor
  return rotatePointForWidget(widget, widgetAnchor(widget, target, portName))
}

function widgetConnectionAnchor(widget, target, portName = null, customAnchor = null) {
  if (isHiddenJunctionWidget(widget)) return widgetCenterPoint(widget)
  if (widget && customAnchor && Number.isFinite(Number(customAnchor.x)) && Number.isFinite(Number(customAnchor.y))) {
    return rotatePointForWidget(widget, {
      x: (Number(widget.x) || 0) + (Number(widget.w) || 0) * Math.max(0, Math.min(1, Number(customAnchor.x))),
      y: (Number(widget.y) || 0) + (Number(widget.h) || 0) * Math.max(0, Math.min(1, Number(customAnchor.y)))
    })
  }
  return rotatePointForWidget(widget, widgetAnchor(widget, target, portName))
}

function routeModeForWidgets(fromWidget, toWidget) {
  if (!fromWidget || !toWidget) return ''
  const start = widgetAnchor(fromWidget, toWidget)
  const end = widgetAnchor(toWidget, fromWidget)
  const dx = Math.abs(end.x - start.x)
  const dy = Math.abs(end.y - start.y)
  if (dy <= 4 && dx > STRAIGHT_LINE_SNAP) return 'straight-h'
  if (dx <= 4 && dy > STRAIGHT_LINE_SNAP) return 'straight-v'
  return dx >= dy ? 'hv' : 'vh'
}

function compactConnectionPoints(points = []) {
  const clean = points
    .map(p => ({ x: Number(p.x), y: Number(p.y) }))
    .filter(p => Number.isFinite(p.x) && Number.isFinite(p.y))
  if (clean.length < 2) return clean

  const deduped = []
  clean.forEach(point => {
    const previous = deduped[deduped.length - 1]
    if (!previous || Math.abs(previous.x - point.x) > 0.001 || Math.abs(previous.y - point.y) > 0.001) {
      deduped.push(point)
    }
  })

  return deduped.filter((point, index) => {
    if (index === 0 || index === deduped.length - 1) return true
    const previous = deduped[index - 1]
    const next = deduped[index + 1]
    const sameVertical = Math.abs(previous.x - point.x) <= 0.001 && Math.abs(point.x - next.x) <= 0.001
    const sameHorizontal = Math.abs(previous.y - point.y) <= 0.001 && Math.abs(point.y - next.y) <= 0.001
    const reverseVertical = sameVertical && (point.y - previous.y) * (next.y - point.y) < -0.001
    const reverseHorizontal = sameHorizontal && (point.x - previous.x) * (next.x - point.x) < -0.001
    return !((sameVertical || sameHorizontal) && !reverseVertical && !reverseHorizontal)
  })
}

function orthogonalizeConnectionPoints(points = []) {
  const clean = compactConnectionPoints(points)
  if (clean.length < 2) return clean
  const result = [clean[0]]
  for (let i = 1; i < clean.length; i++) {
    const previous = result[result.length - 1]
    const current = clean[i]
    const sameX = Math.abs(previous.x - current.x) <= 0.001
    const sameY = Math.abs(previous.y - current.y) <= 0.001
    if (!sameX && !sameY) {
      const before = result[result.length - 2]
      const previousWasVertical = before && Math.abs(before.x - previous.x) <= 0.001
      result.push(previousWasVertical
        ? { x: current.x, y: previous.y }
        : { x: previous.x, y: current.y }
      )
    }
    result.push(current)
  }
  return compactConnectionPoints(result)
}

function routeResult(points, mode, handle = null, orthogonalize = false) {
  const clean = compactConnectionPoints(orthogonalize ? orthogonalizeConnectionPoints(points) : points)
  return {
    path: pointsPath(clean),
    points: clean,
    handle: handle || clean[Math.floor(clean.length / 2)] || clean[0] || { x: 0, y: 0 },
    mode
  }
}

const ROUTE_AVOID_PADDING = 10
const ROUTE_AVOID_GAP = 34
const ROUTE_LOCAL_GAP = 18
const ROUTE_LOCAL_DISTANCE = 180

function paddedWidgetBounds(widget, pad = ROUTE_AVOID_PADDING) {
  return vectorObstacleBounds(widget, pad)
}

function rangesOverlap(a1, a2, b1, b2) {
  const minA = Math.min(a1, a2)
  const maxA = Math.max(a1, a2)
  const minB = Math.min(b1, b2)
  const maxB = Math.max(b1, b2)
  return maxA >= minB && maxB >= minA
}

function segmentHitsBounds(a, b, bounds) {
  const horizontal = Math.abs(a.y - b.y) <= 0.001
  const vertical = Math.abs(a.x - b.x) <= 0.001
  if (horizontal) {
    return a.y >= bounds.y && a.y <= bounds.y + bounds.h &&
      rangesOverlap(a.x, b.x, bounds.x, bounds.x + bounds.w)
  }
  if (vertical) {
    return a.x >= bounds.x && a.x <= bounds.x + bounds.w &&
      rangesOverlap(a.y, b.y, bounds.y, bounds.y + bounds.h)
  }
  return false
}

function routeCollisionScore(points = [], conn = {}) {
  const clean = compactConnectionPoints(points)
  let score = 0
  for (let i = 1; i < clean.length; i++) {
    score += Math.abs(clean[i].x - clean[i - 1].x) + Math.abs(clean[i].y - clean[i - 1].y)
  }
  store.widgets.forEach(widget => {
    if (widget.id === conn.from || widget.id === conn.to) return
    const bounds = paddedWidgetBounds(widget)
    for (let i = 1; i < clean.length; i++) {
      if (segmentHitsBounds(clean[i - 1], clean[i], bounds)) {
        score += 100000
      }
    }
  })
  return score
}

function uniqueNumbers(values = []) {
  return [...new Set(values
    .map(v => Number(v))
    .filter(v => Number.isFinite(v))
    .map(v => Math.round(v * 1000) / 1000)
  )]
}

function prioritizedBends(values, midpoint) {
  return uniqueNumbers(values).sort((a, b) => Math.abs(a - midpoint) - Math.abs(b - midpoint)).slice(0, 18)
}

function portDirection(port) {
  return PORT_EXIT_DIRECTIONS[port] || null
}

function isHorizontalPort(port) {
  const direction = portDirection(port)
  return Boolean(direction && direction.x !== 0)
}

function preferredModeForPort(port, fallback = 'hv') {
  const direction = portDirection(port)
  if (direction?.x) return 'hv'
  if (direction?.y) return 'vh'
  return fallback === 'vh' ? 'vh' : 'hv'
}

function offsetPointByPort(point, port, distance = PORT_EXIT_LENGTH) {
  const direction = portDirection(port)
  if (!direction) return { x: Number(point.x) || 0, y: Number(point.y) || 0 }
  return {
    x: (Number(point.x) || 0) + direction.x * distance,
    y: (Number(point.y) || 0) + direction.y * distance
  }
}

function insetPointByPort(point, port, distance = PORT_JOIN_OVERLAP) {
  const direction = portDirection(port)
  if (!direction) return { x: Number(point.x) || 0, y: Number(point.y) || 0 }
  return {
    x: (Number(point.x) || 0) - direction.x * distance,
    y: (Number(point.y) || 0) - direction.y * distance
  }
}

function routeTurnCount(points = []) {
  const clean = compactConnectionPoints(points)
  let turns = 0
  let previousOrientation = ''
  for (let i = 1; i < clean.length; i++) {
    const dx = clean[i].x - clean[i - 1].x
    const dy = clean[i].y - clean[i - 1].y
    const orientation = Math.abs(dx) >= Math.abs(dy) ? 'h' : 'v'
    if (previousOrientation && previousOrientation !== orientation) turns++
    previousOrientation = orientation
  }
  return turns
}

function immediateReversePenalty(points = [], fromPort = '') {
  const direction = portDirection(fromPort)
  if (!direction || points.length < 3) return 0
  const exit = points[1]
  const next = points.slice(2).find(point =>
    Math.abs(point.x - exit.x) > 0.001 || Math.abs(point.y - exit.y) > 0.001
  )
  if (!next) return 0
  const dx = next.x - exit.x
  const dy = next.y - exit.y
  if (direction.x && Math.abs(dy) <= 0.001 && dx * direction.x < -0.001) {
    return 60000 + Math.abs(dx)
  }
  if (direction.y && Math.abs(dx) <= 0.001 && dy * direction.y < -0.001) {
    return 60000 + Math.abs(dy)
  }
  return 0
}

function portAlignedBends(values, exit, port, axis) {
  const direction = portDirection(port)
  const bends = uniqueNumbers(values)
  if (!direction) return bends
  if (axis === 'x' && direction.x) {
    return bends.filter(value => direction.x > 0 ? value >= exit.x - 0.001 : value <= exit.x + 0.001)
  }
  if (axis === 'y' && direction.y) {
    return bends.filter(value => direction.y > 0 ? value >= exit.y - 0.001 : value <= exit.y + 0.001)
  }
  return bends
}

function bendIsForward(value, exit, port, axis) {
  const direction = portDirection(port)
  if (!direction) return true
  if (axis === 'x' && direction.x) {
    return direction.x > 0 ? value >= exit.x - 0.001 : value <= exit.x + 0.001
  }
  if (axis === 'y' && direction.y) {
    return direction.y > 0 ? value >= exit.y - 0.001 : value <= exit.y + 0.001
  }
  return true
}

function nonDegenerateDetourBend(value, exit, entry, axis) {
  const numeric = Number(value)
  if (axis === 'y') {
    const bendY = Number.isFinite(numeric) ? numeric : exit.y + ROUTE_AVOID_GAP
    if (Math.abs(bendY - exit.y) <= 0.001 && Math.abs(bendY - entry.y) <= 0.001) {
      return exit.y + ROUTE_AVOID_GAP
    }
    return bendY
  }
  const bendX = Number.isFinite(numeric) ? numeric : exit.x + ROUTE_AVOID_GAP
  if (Math.abs(bendX - exit.x) <= 0.001 && Math.abs(bendX - entry.x) <= 0.001) {
    return exit.x + ROUTE_AVOID_GAP
  }
  return bendX
}

function routeCandidateScore(points, conn, mode, preferredMode) {
  let score = routeCollisionScore(points, conn)
  score += routeTurnCount(points) * 4
  score += mode === preferredMode ? 0 : 18
  score += immediateReversePenalty(points, conn.fromPort)
  return score
}

function routeHasObstacleCollision(points, conn) {
  return routeCollisionScore(points, conn) >= 100000
}

function routeLength(points = []) {
  const clean = compactConnectionPoints(points)
  let length = 0
  for (let i = 1; i < clean.length; i++) {
    length += Math.abs(clean[i].x - clean[i - 1].x) + Math.abs(clean[i].y - clean[i - 1].y)
  }
  return length
}

function portGapDistance(startExit, endEntry) {
  return Math.abs(endEntry.x - startExit.x) + Math.abs(endEntry.y - startExit.y)
}

function localPriorityRouteCandidates(start, startExit, endEntry, end, conn, preferredMode) {
  const sourceDirection = portDirection(conn.fromPort)
  const renderStart = insetPointByPort(start, conn.fromPort)
  const renderEnd = insetPointByPort(end, conn.toPort)
  const candidates = []

  function add(mode, points, extraPenalty = 0) {
    candidates.push({
      mode,
      points,
      score: routeCandidateScore(points, conn, mode, preferredMode) + extraPenalty
    })
  }

  if (sourceDirection?.x) {
    if (bendIsForward(endEntry.x, startExit, conn.fromPort, 'x')) {
      add('hv', [renderStart, startExit, { x: endEntry.x, y: startExit.y }, endEntry, renderEnd])
    }
    const detourYs = uniqueNumbers([
      startExit.y + ROUTE_LOCAL_GAP,
      startExit.y - ROUTE_LOCAL_GAP,
      endEntry.y + ROUTE_LOCAL_GAP,
      endEntry.y - ROUTE_LOCAL_GAP,
      startExit.y + ROUTE_AVOID_GAP,
      startExit.y - ROUTE_AVOID_GAP
    ])
    detourYs.forEach(y => {
      add('vh', [renderStart, startExit, { x: startExit.x, y }, { x: endEntry.x, y }, endEntry, renderEnd], 6)
    })
    return candidates
  }

  if (sourceDirection?.y) {
    if (bendIsForward(endEntry.y, startExit, conn.fromPort, 'y')) {
      add('vh', [renderStart, startExit, { x: startExit.x, y: endEntry.y }, endEntry, renderEnd])
    }
    const detourXs = uniqueNumbers([
      startExit.x + ROUTE_LOCAL_GAP,
      startExit.x - ROUTE_LOCAL_GAP,
      endEntry.x + ROUTE_LOCAL_GAP,
      endEntry.x - ROUTE_LOCAL_GAP,
      startExit.x + ROUTE_AVOID_GAP,
      startExit.x - ROUTE_AVOID_GAP
    ])
    detourXs.forEach(x => {
      add('hv', [renderStart, startExit, { x, y: startExit.y }, { x, y: endEntry.y }, endEntry, renderEnd], 6)
    })
    return candidates
  }

  add('hv', [renderStart, startExit, { x: endEntry.x, y: startExit.y }, endEntry, renderEnd])
  add('vh', [renderStart, startExit, { x: startExit.x, y: endEntry.y }, endEntry, renderEnd])
  return candidates
}

function directPortRoute(start, startExit, endEntry, end, conn) {
  const sourceDirection = portDirection(conn.fromPort)
  const renderStart = insetPointByPort(start, conn.fromPort)
  const renderEnd = insetPointByPort(end, conn.toPort)
  if (sourceDirection?.x && Math.abs(startExit.y - endEntry.y) <= PORT_ALIGNMENT_SNAP) {
    const points = [renderStart, startExit, endEntry, renderEnd]
    if (!routeHasObstacleCollision(points, conn)) {
      return routeResult(points, 'straight-h', {
        x: startExit.x + (endEntry.x - startExit.x) / 2,
        y: startExit.y
      })
    }
  }

  if (sourceDirection?.y && Math.abs(startExit.x - endEntry.x) <= PORT_ALIGNMENT_SNAP) {
    const points = [renderStart, startExit, endEntry, renderEnd]
    if (!routeHasObstacleCollision(points, conn)) {
      return routeResult(points, 'straight-v', {
        x: startExit.x,
        y: startExit.y + (endEntry.y - startExit.y) / 2
      })
    }
  }

  if (sourceDirection?.x && bendIsForward(endEntry.x, startExit, conn.fromPort, 'x')) {
    const points = [renderStart, startExit, { x: endEntry.x, y: startExit.y }, endEntry, renderEnd]
    if (!routeHasObstacleCollision(points, conn)) {
      return routeResult(points, 'hv', {
        x: startExit.x + (endEntry.x - startExit.x) / 2,
        y: startExit.y
      })
    }
  }

  if (sourceDirection?.y && bendIsForward(endEntry.y, startExit, conn.fromPort, 'y')) {
    const points = [renderStart, startExit, { x: startExit.x, y: endEntry.y }, endEntry, renderEnd]
    if (!routeHasObstacleCollision(points, conn)) {
      return routeResult(points, 'vh', {
        x: startExit.x,
        y: startExit.y + (endEntry.y - startExit.y) / 2
      })
    }
  }

  return null
}

function chooseDirectionAwareRoute(start, end, conn = {}, preferredMode = null) {
  const startExit = offsetPointByPort(start, conn.fromPort)
  const endEntry = offsetPointByPort(end, conn.toPort)
  const renderStart = insetPointByPort(start, conn.fromPort)
  const renderEnd = insetPointByPort(end, conn.toPort)
  if (preferredMode === 'straight') {
    return routeForMode(start, end, preferredMode, conn)
  }
  const modePreference = preferredModeForPort(conn.fromPort, preferredMode || routeModeForPoints(startExit, endEntry))
  const directRoute = directPortRoute(start, startExit, endEntry, end, conn)
  if (directRoute) return directRoute

  const isLocalRoute = portGapDistance(startExit, endEntry) <= ROUTE_LOCAL_DISTANCE

  if (isLocalRoute) {
    const localBest = localPriorityRouteCandidates(start, startExit, endEntry, end, conn, modePreference)
      .filter(candidate => !routeHasObstacleCollision(candidate.points, conn))
      .sort((a, b) => a.score - b.score)[0]
    if (localBest) {
      return routeResult(
        localBest.points,
        localBest.mode,
        localBest.points[Math.floor(localBest.points.length / 2)] || startExit
      )
    }
  }

  const midpointX = startExit.x + (endEntry.x - startExit.x) / 2
  const midpointY = startExit.y + (endEntry.y - startExit.y) / 2
  const horizontalDirection = endEntry.x >= startExit.x ? 1 : -1
  const verticalDirection = endEntry.y >= startExit.y ? 1 : -1
  const bendXs = [
    midpointX,
    endEntry.x,
    startExit.x + ROUTE_LOCAL_GAP,
    startExit.x - ROUTE_LOCAL_GAP,
    endEntry.x + ROUTE_LOCAL_GAP,
    endEntry.x - ROUTE_LOCAL_GAP,
    startExit.x + horizontalDirection * ROUTE_AVOID_GAP,
    endEntry.x - horizontalDirection * ROUTE_AVOID_GAP
  ]
  const bendYs = [
    midpointY,
    endEntry.y,
    startExit.y + ROUTE_LOCAL_GAP,
    startExit.y - ROUTE_LOCAL_GAP,
    endEntry.y + ROUTE_LOCAL_GAP,
    endEntry.y - ROUTE_LOCAL_GAP,
    startExit.y + verticalDirection * ROUTE_AVOID_GAP,
    endEntry.y - verticalDirection * ROUTE_AVOID_GAP
  ]

  store.widgets.forEach(widget => {
    if (widget.id === conn.from || widget.id === conn.to) return
    const b = paddedWidgetBounds(widget, ROUTE_AVOID_PADDING)
    bendXs.push(b.x - ROUTE_AVOID_GAP, b.x + b.w + ROUTE_AVOID_GAP)
    bendYs.push(b.y - ROUTE_AVOID_GAP, b.y + b.h + ROUTE_AVOID_GAP)
  })

  const prioritizedXs = prioritizedBends(
    portAlignedBends(bendXs, startExit, conn.fromPort, 'x'),
    modePreference === 'hv' ? endEntry.x : midpointX
  )
  const prioritizedYs = prioritizedBends(
    portAlignedBends(bendYs, startExit, conn.fromPort, 'y'),
    modePreference === 'vh' ? endEntry.y : midpointY
  )
  const modes = modePreference === 'vh' ? ['vh', 'hv'] : ['hv', 'vh']
  const candidates = []

  function addCandidate(mode, bend) {
    const points = mode === 'hv'
      ? [renderStart, startExit, { x: bend, y: startExit.y }, { x: bend, y: endEntry.y }, endEntry, renderEnd]
      : [renderStart, startExit, { x: startExit.x, y: bend }, { x: endEntry.x, y: bend }, endEntry, renderEnd]
    candidates.push({
      mode,
      bend,
      points,
      score: routeCandidateScore(points, conn, mode, modePreference)
    })
  }

  modes.forEach(mode => {
    if (mode === 'hv') {
      prioritizedXs.forEach(bendX => addCandidate('hv', bendX))
    } else {
      prioritizedYs.forEach(bendY => addCandidate('vh', bendY))
    }
  })

  const localBest = isLocalRoute
    ? candidates
        .filter(candidate => routeLength(candidate.points) <= ROUTE_LOCAL_DISTANCE * 2.5 && candidate.score < 100000)
        .sort((a, b) => a.score - b.score)[0]
    : null
  const best = localBest || candidates.sort((a, b) => a.score - b.score)[0]
  if (!best) {
    return routeResult([renderStart, startExit, endEntry, renderEnd], modePreference, {
      x: startExit.x + (endEntry.x - startExit.x) / 2,
      y: startExit.y + (endEntry.y - startExit.y) / 2
    }, true)
  }
  return routeResult(
    best.points,
    best.mode,
    best.mode === 'hv'
      ? { x: best.bend, y: startExit.y + (endEntry.y - startExit.y) / 2 }
      : { x: startExit.x + (endEntry.x - startExit.x) / 2, y: best.bend }
  )
}

function routeModeForPoints(start, end) {
  const dx = Math.abs(end.x - start.x)
  const dy = Math.abs(end.y - start.y)
  if (dy <= 4 && dx > STRAIGHT_LINE_SNAP) return 'straight-h'
  if (dx <= 4 && dy > STRAIGHT_LINE_SNAP) return 'straight-v'
  return dx >= dy ? 'hv' : 'vh'
}

function routeForMode(start, end, mode, conn = {}) {
  if (conn.fromPort || conn.toPort) {
    const startExit = offsetPointByPort(start, conn.fromPort)
    const endEntry = offsetPointByPort(end, conn.toPort)
    const renderStart = insetPointByPort(start, conn.fromPort)
    const renderEnd = insetPointByPort(end, conn.toPort)
    const sourceDirection = portDirection(conn.fromPort)
    if (mode === 'straight') {
      return routeResult([renderStart, startExit, endEntry, renderEnd], mode, {
        x: startExit.x + (endEntry.x - startExit.x) / 2,
        y: startExit.y + (endEntry.y - startExit.y) / 2
      })
    }
    if (mode === 'straight-h' || mode === 'straight-v') {
      return routeResult([renderStart, startExit, endEntry, renderEnd], mode, {
        x: startExit.x + (endEntry.x - startExit.x) / 2,
        y: startExit.y + (endEntry.y - startExit.y) / 2
      })
    }

    if (mode === 'hv') {
      let bendX = Number.isFinite(Number(conn.bendX))
        ? Number(conn.bendX)
        : startExit.x + (endEntry.x - startExit.x) / 2
      if (sourceDirection?.x && !bendIsForward(bendX, startExit, conn.fromPort, 'x')) {
        bendX = startExit.x + sourceDirection.x * PORT_EXIT_LENGTH
      }
      bendX = nonDegenerateDetourBend(bendX, startExit, endEntry, 'x')
      return routeResult(
        [renderStart, startExit, { x: bendX, y: startExit.y }, { x: bendX, y: endEntry.y }, endEntry, renderEnd],
        'hv',
        { x: bendX, y: startExit.y + (endEntry.y - startExit.y) / 2 }
      )
    }

    let bendY = Number.isFinite(Number(conn.bendY))
      ? Number(conn.bendY)
      : startExit.y + (endEntry.y - startExit.y) / 2
    if (sourceDirection?.y && !bendIsForward(bendY, startExit, conn.fromPort, 'y')) {
      bendY = startExit.y + sourceDirection.y * PORT_EXIT_LENGTH
    }
    bendY = nonDegenerateDetourBend(bendY, startExit, endEntry, 'y')
    return routeResult(
      [renderStart, startExit, { x: startExit.x, y: bendY }, { x: endEntry.x, y: bendY }, endEntry, renderEnd],
      'vh',
      { x: startExit.x + (endEntry.x - startExit.x) / 2, y: bendY }
    )
  }

  if (mode === 'straight') {
    return routeResult([start, end], mode, {
      x: start.x + (end.x - start.x) / 2,
      y: start.y + (end.y - start.y) / 2
    })
  }
  if (mode === 'straight-h' || mode === 'straight-v') {
    return routeResult([start, end], mode, {
      x: start.x + (end.x - start.x) / 2,
      y: start.y + (end.y - start.y) / 2
    })
  }
  if (mode === 'hv') {
    const bendX = Number.isFinite(Number(conn.bendX)) ? Number(conn.bendX) : start.x + (end.x - start.x) / 2
    return routeResult(
      [start, { x: bendX, y: start.y }, { x: bendX, y: end.y }, end],
      'hv',
      { x: bendX, y: start.y + (end.y - start.y) / 2 }
    )
  }
  const bendY = Number.isFinite(Number(conn.bendY)) ? Number(conn.bendY) : start.y + (end.y - start.y) / 2
  return routeResult(
    [start, { x: start.x, y: bendY }, { x: end.x, y: bendY }, end],
    'vh',
    { x: start.x + (end.x - start.x) / 2, y: bendY }
  )
}

function chooseAutoRoute(start, end, preferredMode, conn = {}) {
  if (conn.fromPort || conn.toPort) {
    return chooseDirectionAwareRoute(start, end, conn, preferredMode)
  }
  if (preferredMode === 'straight') {
    return routeForMode(start, end, preferredMode, conn)
  }
  if (preferredMode === 'straight-h' || preferredMode === 'straight-v') {
    return routeForMode(start, end, preferredMode, conn)
  }

  const midpointX = start.x + (end.x - start.x) / 2
  const midpointY = start.y + (end.y - start.y) / 2
  const directionX = end.x >= start.x ? 1 : -1
  const directionY = end.y >= start.y ? 1 : -1
  const bendXs = [midpointX, start.x + directionX * ROUTE_AVOID_GAP, end.x - directionX * ROUTE_AVOID_GAP]
  const bendYs = [midpointY, start.y + directionY * ROUTE_AVOID_GAP, end.y - directionY * ROUTE_AVOID_GAP]

  store.widgets.forEach(widget => {
    if (widget.id === conn.from || widget.id === conn.to) return
    const b = paddedWidgetBounds(widget, ROUTE_AVOID_PADDING)
    bendXs.push(b.x - ROUTE_AVOID_GAP, b.x + b.w + ROUTE_AVOID_GAP)
    bendYs.push(b.y - ROUTE_AVOID_GAP, b.y + b.h + ROUTE_AVOID_GAP)
  })

  const modes = preferredMode === 'vh' ? ['vh', 'hv'] : ['hv', 'vh']
  const candidates = []
  modes.forEach(mode => {
    if (mode === 'hv') {
      prioritizedBends(bendXs, midpointX).forEach(bendX => {
        const points = [start, { x: bendX, y: start.y }, { x: bendX, y: end.y }, end]
        candidates.push({ mode, bendX, points, score: routeCollisionScore(points, conn) })
      })
    } else {
      prioritizedBends(bendYs, midpointY).forEach(bendY => {
        const points = [start, { x: start.x, y: bendY }, { x: end.x, y: bendY }, end]
        candidates.push({ mode, bendY, points, score: routeCollisionScore(points, conn) })
      })
    }
  })

  const best = candidates.sort((a, b) => a.score - b.score)[0]
  if (!best) return routeForMode(start, end, preferredMode, conn)
  return routeResult(
    best.points,
    best.mode,
    best.mode === 'hv'
      ? { x: best.bendX, y: start.y + (end.y - start.y) / 2 }
      : { x: start.x + (end.x - start.x) / 2, y: best.bendY }
  )
}

function routeForWidgets(fromWidget, toWidget, conn = {}) {
  if (!fromWidget || !toWidget) return { path: '', handle: { x: 0, y: 0 }, mode: 'hv' }
  const explicitStart = connectionWidgetAnchor(fromWidget, toWidget, conn.fromPort, conn.fromAnchor)
  const explicitEnd = connectionWidgetAnchor(toWidget, fromWidget, conn.toPort, conn.toAnchor)
  const visualStart = insetPointByPort(explicitStart, conn.fromPort)
  const visualEnd = insetPointByPort(explicitEnd, conn.toPort)
  if (Array.isArray(conn.waypoints) && conn.waypoints.length > 0) {
    const hasPortRoute = conn.fromPort || conn.toPort
    const manualPoints = conn.waypoints
      .map(p => ({ x: Number(p.x), y: Number(p.y) }))
      .filter(p => Number.isFinite(p.x) && Number.isFinite(p.y))
    const points = hasPortRoute
      ? [
          visualStart,
          offsetPointByPort(explicitStart, conn.fromPort),
          ...manualPoints,
          offsetPointByPort(explicitEnd, conn.toPort),
          visualEnd
        ]
      : [explicitStart, ...manualPoints, explicitEnd]
    if (hasPortRoute && !conn.manualRoute) {
      const autoRoute = chooseAutoRoute(
        explicitStart,
        explicitEnd,
        routeModeForWidgets(fromWidget, toWidget),
        { ...conn, waypoints: null, routeMode: null, bendX: null, bendY: null }
      )
      if (autoRoute?.points?.length &&
          !routeHasObstacleCollision(autoRoute.points, conn) &&
          routeLength(points) > routeLength(autoRoute.points) * 1.5 + 40) {
        return autoRoute
      }
    }
    return routeResult(points, 'waypoints', points[Math.floor(points.length / 2)] || visualStart, true)
  }
  const mode = conn.manualRoute && conn.routeMode
    ? conn.routeMode
    : routeModeForWidgets(fromWidget, toWidget)
  const hasManualBend = Boolean(conn.manualRoute) && (
    Number.isFinite(Number(conn.bendX)) ||
    Number.isFinite(Number(conn.bendY)) ||
    !!conn.routeMode
  )
  if (hasManualBend) return routeForMode(explicitStart, explicitEnd, mode, conn)
  return chooseAutoRoute(explicitStart, explicitEnd, mode, conn)
}

function pointsPath(points = []) {
  const clean = compactConnectionPoints(points)
  if (clean.length < 2) return ''
  return clean.map((p, index) => `${index === 0 ? 'M' : 'L'} ${p.x} ${p.y}`).join(' ')
}

function rawConnectionPoints(conn) {
  return getRenderedConnectionBase(conn).points || []
}

function renderConnectionPoints(conn) {
  return compactConnectionPoints(rawConnectionPoints(conn))
}

function connectionPath(conn) {
  return pointsPath(renderConnectionPoints(conn))
}

function connectionHandle(conn) {
  return getRenderedConnectionBase(conn).handle
}

function connectionHandleVisible(conn) {
  return getRenderedConnectionBase(conn).handleVisible
}

function connectionWaypointHandles(conn) {
  if (selectedConnectionId.value !== conn.id) return []
  if (Array.isArray(conn.points) && conn.points.length > 2) {
    return conn.points.slice(1, -1).map((point, index) => ({
      index,
      x: Number(point.x),
      y: Number(point.y)
    })).filter(point => Number.isFinite(point.x) && Number.isFinite(point.y))
  }
  if (!Array.isArray(conn.waypoints) || conn.waypoints.length === 0) return []
  return conn.waypoints.map((point, index) => ({
    index,
    x: Number(point.x),
    y: Number(point.y)
  })).filter(point => Number.isFinite(point.x) && Number.isFinite(point.y))
}

function isWaypointAlignmentHot(conn, index) {
  return activeAlignmentHandle.value?.type === 'waypoint' &&
    activeAlignmentHandle.value?.connId === conn.id &&
    activeAlignmentHandle.value?.index === index
}

function connectionControlPoints(conn) {
  if (Array.isArray(conn.points) && conn.points.length >= 2 && (!conn.from || !conn.to)) {
    return conn.points.map(p => ({ x: Number(p.x), y: Number(p.y) }))
  }
  const fromWidget = widgetMap.value[conn.from]
  const toWidget = widgetMap.value[conn.to]
  if (!fromWidget || !toWidget) return []
  const start = connectionWidgetAnchor(fromWidget, toWidget, conn.fromPort, conn.fromAnchor)
  const end = connectionWidgetAnchor(toWidget, fromWidget, conn.toPort, conn.toAnchor)
  return [
    start,
    ...(Array.isArray(conn.waypoints) ? conn.waypoints.map(p => ({ x: Number(p.x), y: Number(p.y) })) : []),
    end
  ]
}

function connectionInsertHandles(conn) {
  if (selectedConnectionId.value !== conn.id) return []
  const points = renderConnectionPoints(conn)
  if (points.length < 2) return []
  const handles = []
  for (let i = 0; i < points.length - 1; i++) {
    const a = points[i]
    const b = points[i + 1]
    const length = Math.hypot(b.x - a.x, b.y - a.y)
    if (length < 44) continue
    handles.push({
      segmentIndex: i,
      x: a.x + (b.x - a.x) / 2,
      y: a.y + (b.y - a.y) / 2
    })
  }
  return handles
}

function pointOnSegmentFromCursor(cursor, a, b) {
  const horizontal = Math.abs(a.y - b.y) <= Math.abs(a.x - b.x)
  if (horizontal) {
    return {
      x: Math.max(Math.min(a.x, b.x), Math.min(Math.max(a.x, b.x), cursor.x)),
      y: a.y
    }
  }
  return {
    x: a.x,
    y: Math.max(Math.min(a.y, b.y), Math.min(Math.max(a.y, b.y), cursor.y))
  }
}

function segmentDistance(point, a, b) {
  const vx = b.x - a.x
  const vy = b.y - a.y
  const len2 = vx * vx + vy * vy
  if (!len2) return Math.hypot(point.x - a.x, point.y - a.y)
  const t = Math.max(0, Math.min(1, ((point.x - a.x) * vx + (point.y - a.y) * vy) / len2))
  const px = a.x + vx * t
  const py = a.y + vy * t
  return Math.hypot(point.x - px, point.y - py)
}

function nearestSegmentIndex(points, point) {
  if (points.length < 2) return -1
  let best = 0
  let bestDistance = Infinity
  for (let i = 0; i < points.length - 1; i++) {
    const distance = segmentDistance(point, points[i], points[i + 1])
    if (distance < bestDistance) {
      best = i
      bestDistance = distance
    }
  }
  return best
}

function materializeConnectionWaypoints(conn) {
  if (Array.isArray(conn.points) && conn.points.length >= 2 && (!conn.from || !conn.to)) {
    return conn.points.slice(1, -1).map(p => ({ x: Number(p.x), y: Number(p.y) }))
  }
  const routePoints = renderConnectionPoints(conn)
  return routePoints.slice(1, -1).map(p => ({ x: Number(p.x), y: Number(p.y) }))
}

function updateConnectionWaypointById(connectionId, index, point) {
  const current = store.connections.find(c => c.id === connectionId)
  if (!current) return
  const nextPoint = { x: Number(point.x) || 0, y: Number(point.y) || 0 }
  if (Array.isArray(current.points) && current.points.length >= 2 && (!current.from || !current.to)) {
    const points = current.points.map(p => ({ x: Number(p.x), y: Number(p.y) }))
    const actualIndex = index + 1
    if (!points[actualIndex]) return
    points[actualIndex] = nextPoint
    store.updateConnection(connectionId, { points, manualRoute: true })
    return
  }
  const waypoints = Array.isArray(current.waypoints)
    ? current.waypoints.map(p => ({ x: Number(p.x), y: Number(p.y) }))
    : []
  if (!waypoints[index]) return
  waypoints[index] = nextPoint
    store.updateConnection(connectionId, {
      waypoints,
      manualRoute: true,
      routeMode: null,
      bendX: null,
      bendY: null
  })
}

function startConnectionWaypointDragById(connectionId, index, e, initialPoint = null) {
  selectConnection(connectionId)
  const snapPointForEvent = (ev, forceRound = false) => {
    const current = store.connections.find(c => c.id === connectionId)
    if (!current) return canvasPointFromEvent(ev)
    const controlPoints = connectionControlPoints(current)
    const anchors = [controlPoints[index], controlPoints[index + 2]].filter(Boolean)
    const raw = canvasPointFromEvent(ev)
    const lineSnap = smartWaypointSnap(raw, controlPoints[index], controlPoints[index + 2], ev)
    const snap = lineSnap || smartOrthogonalSnap(raw, anchors, ev)
    applyAlignmentFeedback(snap, { type: 'waypoint', connId: connectionId, index })
    const point = snap.snapped ? snap.point : raw
    return forceRound ? roundPoint(point) : point
  }
  const onMove = (ev) => {
    updateConnectionWaypointById(connectionId, index, snapPointForEvent(ev))
  }
  const onUp = (ev) => {
    updateConnectionWaypointById(connectionId, index, snapPointForEvent(ev, true))
    clearAlignmentFeedback()
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  if (initialPoint) updateConnectionWaypointById(connectionId, index, roundPoint(initialPoint))
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function startConnectionWaypointDrag(conn, index, e) {
  startConnectionWaypointDragById(conn.id, index, e)
}

function insertConnectionWaypoint(conn, segmentIndex, point) {
  if (Array.isArray(conn.points) && conn.points.length >= 2 && (!conn.from || !conn.to)) {
    const points = conn.points.map(p => ({ x: Number(p.x), y: Number(p.y) }))
    const insertIndex = Math.max(1, Math.min(points.length - 1, segmentIndex + 1))
    points.splice(insertIndex, 0, point)
    store.updateConnection(conn.id, { points, manualRoute: true })
    return insertIndex - 1
  }
  const waypoints = materializeConnectionWaypoints(conn)
  const insertIndex = Math.max(0, Math.min(waypoints.length, segmentIndex))
  waypoints.splice(insertIndex, 0, point)
  store.updateConnection(conn.id, {
    waypoints,
    manualRoute: true,
    routeMode: null,
    bendX: null,
    bendY: null
  })
  return insertIndex
}

function startConnectionInsertDrag(conn, segmentIndex, e) {
  selectConnection(conn.id)
  const basePoints = renderConnectionPoints(conn).map(point => ({
    x: Number(point.x),
    y: Number(point.y)
  }))
  const a = basePoints[segmentIndex]
  const b = basePoints[segmentIndex + 1]
  if (!a || !b) return
  const horizontal = Math.abs(b.x - a.x) >= Math.abs(b.y - a.y)
  const startCursor = canvasPointFromEvent(e)
  const startCoordinate = horizontal ? a.y : a.x

  const endpointExit = (endpoint) => {
    const fromWidget = widgetMap.value[conn.from]
    const toWidget = widgetMap.value[conn.to]
    if (!fromWidget || !toWidget) return null
    if (endpoint === 'start') {
      const anchor = connectionWidgetAnchor(fromWidget, toWidget, conn.fromPort, conn.fromAnchor)
      return offsetPointByPort(anchor, conn.fromPort)
    }
    const anchor = connectionWidgetAnchor(toWidget, fromWidget, conn.toPort, conn.toAnchor)
    return offsetPointByPort(anchor, conn.toPort)
  }

  const moveSegment = (coordinate) => {
    const points = basePoints.map(point => ({ ...point }))
    const lastIndex = points.length - 1
    const movedA = horizontal
      ? { x: a.x, y: coordinate }
      : { x: coordinate, y: a.y }
    const movedB = horizontal
      ? { x: b.x, y: coordinate }
      : { x: coordinate, y: b.y }

    if (segmentIndex === 0 && conn.from && conn.to) {
      const exit = endpointExit('start')
      if (exit) {
        if (horizontal) movedA.x = exit.x
        else movedA.y = exit.y
      }
    }
    if (segmentIndex + 1 === lastIndex && conn.from && conn.to) {
      const entry = endpointExit('end')
      if (entry) {
        if (horizontal) movedB.x = entry.x
        else movedB.y = entry.y
      }
    }

    const moved = [
      ...points.slice(0, segmentIndex),
      ...(segmentIndex === 0 ? [points[0], movedA] : [movedA]),
      movedB,
      ...(segmentIndex + 1 === lastIndex ? [points[lastIndex]] : points.slice(segmentIndex + 2))
    ].map(roundPoint)

    if (conn.from && conn.to) {
      store.updateConnection(conn.id, {
        waypoints: compactConnectionPoints(moved).slice(1, -1),
        points: null,
        manualRoute: true,
        routeMode: null,
        bendX: null,
        bendY: null
      })
    } else {
      store.updateConnection(conn.id, {
        points: compactConnectionPoints(moved),
        manualRoute: true
      })
    }
  }

  const updateFromEvent = (ev) => {
    const cursor = canvasPointFromEvent(ev)
    const delta = horizontal ? cursor.y - startCursor.y : cursor.x - startCursor.x
    const coordinate = Math.round(startCoordinate + delta)
    moveSegment(coordinate)
    orthogonalGuide.value = horizontal
      ? { orientation: 'horizontal', value: coordinate }
      : { orientation: 'vertical', value: coordinate }
    activeAlignmentHandle.value = {
      type: 'segment',
      connId: conn.id,
      segmentIndex
    }
  }

  const onMove = (ev) => updateFromEvent(ev)
  const onUp = (ev) => {
    updateFromEvent(ev)
    clearAlignmentFeedback()
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function insertConnectionWaypointAtCursor(conn, e) {
  selectConnection(conn.id)
  const points = renderConnectionPoints(conn)
  if (points.length < 2) return
  const cursor = canvasPointFromEvent(e)
  const segmentIndex = nearestSegmentIndex(points, cursor)
  if (segmentIndex < 0) return
  const point = pointOnSegmentFromCursor(cursor, points[segmentIndex], points[segmentIndex + 1])
  insertConnectionWaypoint(conn, segmentIndex, point)
}

function applicationConnectionPath(conn) {
  const b = applicationBounds.value
  const shiftedPoints = renderConnectionPoints(conn).map(p => ({
    x: Number(p.x) - b.minX + b.margin,
    y: Number(p.y) - b.minY + b.margin
  }))
  return pointsPath(compactConnectionPoints(shiftedPoints))
}

function canvasPointFromEvent(e) {
  const viewport = canvasAreaRef.value?.querySelector('.canvas-viewport')
  if (!viewport) return { x: 0, y: 0 }
  const rect = viewport.getBoundingClientRect()
  const z = Number(zoom.value) || 1
  return {
    x: (e.clientX - rect.left) / z - editorCanvasBounds.value.offsetX,
    y: (e.clientY - rect.top) / z - editorCanvasBounds.value.offsetY
  }
}

function startConnectionBendDrag(conn, e) {
  if (Array.isArray(conn.points) || Array.isArray(conn.waypoints)) return
  selectConnection(conn.id)
  const fromWidget = widgetMap.value[conn.from]
  const toWidget = widgetMap.value[conn.to]
  const renderedMode = getRenderedConnectionBase(conn)?.mode
  const mode = renderedMode === 'hv' || renderedMode === 'vh'
    ? renderedMode
    : (conn.routeMode || routeModeForWidgets(fromWidget, toWidget))
  const start = connectionWidgetAnchor(fromWidget, toWidget, conn.fromPort, conn.fromAnchor)
  const end = connectionWidgetAnchor(toWidget, fromWidget, conn.toPort, conn.toAnchor)
  const onMove = (ev) => {
    const p = canvasPointFromEvent(ev)
    if (mode === 'hv') {
      orthogonalGuide.value = { orientation: 'vertical', value: p.x }
      activeAlignmentHandle.value = { type: 'bend', connId: conn.id }
      store.updateConnection(conn.id, { manualRoute: true, routeMode: mode, bendX: p.x, bendY: null })
    } else {
      orthogonalGuide.value = { orientation: 'horizontal', value: p.y }
      activeAlignmentHandle.value = { type: 'bend', connId: conn.id }
      store.updateConnection(conn.id, { manualRoute: true, routeMode: mode, bendX: null, bendY: p.y })
    }
  }
  const onUp = (ev) => {
    const p = roundPoint(canvasPointFromEvent(ev))
    if (mode === 'hv') {
      store.updateConnection(conn.id, { manualRoute: true, routeMode: mode, bendX: p.x, bendY: null })
    } else {
      store.updateConnection(conn.id, { manualRoute: true, routeMode: mode, bendX: null, bendY: p.y })
    }
    clearAlignmentFeedback()
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  if (mode === 'hv') orthogonalGuide.value = { orientation: 'vertical', value: start.x + (end.x - start.x) / 2 }
  else orthogonalGuide.value = { orientation: 'horizontal', value: start.y + (end.y - start.y) / 2 }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function selectPastedWidget(widget) {
  if (!widget) return
  selectedId.value = widget.id
  selectedConnectionId.value = null
  configWidgetId.value = null
  connectionDraftFrom.value = null
}

function copyWidgetById(id) {
  const widget = store.widgets.find(item => item.id === id)
  if (!widget) return false
  copiedWidget.value = JSON.parse(JSON.stringify(widget))
  copiedSelection.value = null
  pasteSequence = 0
  showConnectionNotice(`已复制“${widget.config?.title || typeLabel(widget.type)}”`)
  return true
}

function pasteCopiedWidget(point = null) {
  if (copiedSelection.value?.widgets?.length) return pasteCopiedSelection(point)
  if (!copiedWidget.value) return null
  const source = copiedWidget.value
  pasteSequence += 1
  const offset = 24 * pasteSequence
  const atCursor = point && Number.isFinite(Number(point.x)) && Number.isFinite(Number(point.y))
  const x = atCursor
    ? Number(point.x) - (Number(source.w) || 0) / 2
    : (Number(source.x) || 0) + offset
  const y = atCursor
    ? Number(point.y) - (Number(source.h) || 0) / 2
    : (Number(source.y) || 0) + offset
  const pasted = store.duplicateWidget(source, Math.round(x), Math.round(y))
  if (pasted) {
    selectPastedWidget(pasted)
    showConnectionNotice(`已粘贴“${pasted.config?.title || typeLabel(pasted.type)}”`)
  }
  return pasted
}

function duplicateWidgetById(id) {
  const source = store.widgets.find(item => item.id === id)
  if (!source) return null
  const pasted = store.duplicateWidget(source, (Number(source.x) || 0) + 24, (Number(source.y) || 0) + 24)
  if (pasted) {
    selectPastedWidget(pasted)
    showConnectionNotice(`已创建“${pasted.config?.title || typeLabel(pasted.type)}”副本`)
  }
  return pasted
}

function closeContextMenu() {
  contextMenu.open = false
}

function openPlatformContextMenu(e) {
  const target = e.target instanceof Element ? e.target : null
  if (!target) return
  if (target.closest('.platform-context-menu')) {
    e.preventDefault()
    return
  }
  if (!target.closest('.canvas-page, .application-overlay')) return

  e.preventDefault()
  let targetType = 'canvas'
  let targetId = null
  let canvasPoint = null

  if (applicationMode.value) {
    targetType = 'application'
  } else {
    const widgetElement = target.closest('.widget-wrapper')
    const connectionElement = target.closest('.connection-group')
    if (widgetElement?.dataset.widgetId) {
      targetType = 'widget'
      targetId = widgetElement.dataset.widgetId
      selectWidget(targetId)
    } else if (connectionElement?.dataset.connectionId) {
      targetType = 'connection'
      targetId = connectionElement.dataset.connectionId
      selectConnection(targetId)
    } else if (target.closest('.canvas-area')) {
      canvasPoint = canvasPointFromEvent(e)
    }
  }

  const menuHeight = targetType === 'widget' ? 336 : targetType === 'connection' ? 226 : 270
  contextMenu.targetType = targetType
  contextMenu.targetId = targetId
  contextMenu.canvasPoint = canvasPoint
  contextMenu.x = Math.max(8, Math.min(e.clientX, window.innerWidth - 236))
  contextMenu.y = Math.max(8, Math.min(e.clientY, window.innerHeight - menuHeight - 8))
  contextMenu.open = true
}

function closeContextMenuFromOutside(e) {
  if (!contextMenu.open) return
  if (contextMenuRef.value?.contains(e.target)) return
  closeContextMenu()
}

function runContextAction(action) {
  const targetId = contextMenu.targetId
  const point = contextMenu.canvasPoint
  closeContextMenu()

  if (action === 'edit-widget') openConfig(targetId)
  else if (action === 'copy-widget') copyWidgetById(targetId)
  else if (action === 'duplicate-widget') duplicateWidgetById(targetId)
  else if (action === 'delete-widget') {
    const widget = store.widgets.find(item => item.id === targetId)
    if (widget) openWidgetDelete(widget)
  } else if (action === 'paste-widget') pasteCopiedWidget(point)
  else if (action === 'undo') {
    if (store.undoLayout()) resetSelectionAfterHistory()
  } else if (action === 'redo') {
    if (store.redoLayout()) resetSelectionAfterHistory()
  } else if (action === 'auto-route') {
    selectedConnectionId.value = targetId
    resetSelectedConnectionRoute()
  } else if (action === 'delete-connection') {
    selectedConnectionId.value = targetId
    removeSelectedConnection()
  } else if (action === 'focus-content') focusCanvasContent()
  else if (action === 'clear-selection') clearSelection()
  else if (action === 'enter-application') enterApplicationMode()
  else if (action === 'exit-application') exitApplicationMode()
}

function handleKeyDown(e) {
  const target = e.target
  const shortcut = e.ctrlKey || e.metaKey
  const key = String(e.key || '').toLowerCase()
  if (e.key === 'Escape') {
    if (contextMenu.open) closeContextMenu()
    else if (applicationMode.value) exitApplicationMode()
    return
  }
  if (shortcut && key === 's') {
    e.preventDefault()
    saveCurrentCanvas()
    return
  }
  if (target && ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)) return
  if (applicationMode.value) return

  if (shortcut && key === 'z') {
    e.preventDefault()
    const changed = e.shiftKey ? store.redoLayout() : store.undoLayout()
    if (changed) resetSelectionAfterHistory()
    return
  }
  if (shortcut && key === 'y') {
    e.preventDefault()
    if (store.redoLayout()) resetSelectionAfterHistory()
    return
  }
  if (shortcut && key === 'c') {
    if (selectedWidgetIds.value.length) {
      e.preventDefault()
      copySelectedWidgets()
      return
    }
    if (!copyWidgetById(selectedId.value)) return
    e.preventDefault()
    return
  }
  if (shortcut && key === 'v') {
    if (!copiedWidget.value && !copiedSelection.value?.widgets?.length) return
    e.preventDefault()
    pasteCopiedWidget()
    return
  }
  if (shortcut && key === 'd') {
    if (!selectedId.value) return
    e.preventDefault()
    duplicateWidgetById(selectedId.value)
    return
  }
  if (e.code === 'Space') {
    e.preventDefault()
    focusCanvasContent()
    return
  }
  if (e.key !== 'Delete' && e.key !== 'Backspace') return
  if (selectedWidgetIds.value.length) {
    e.preventDefault()
    deleteSelectedWidgets()
  } else if (selectedConnectionId.value) {
    e.preventDefault()
    removeSelectedConnection()
  } else if (selectedId.value) {
    const widget = store.widgets.find(item => item.id === selectedId.value)
    if (widget) {
      e.preventDefault()
      openWidgetDelete(widget)
    }
  }
}

function resetSelectionAfterHistory() {
  selectedId.value = null
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
  configWidgetId.value = null
  connectionDraftFrom.value = null
  Object.keys(configDrafts).forEach(key => delete configDrafts[key])
}

function openConfig(id) {
  const latest = store.widgets.find(w => w.id === id)
  if (!latest) return
  selectedId.value = latest.id
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
  connectionDraftFrom.value = null
  configWidgetId.value = latest.id
  ensureConfigDraft(latest, true)
}

function applyConfig() {
  if (configWidget.value) {
    const widgetId = configWidget.value.id
    const draft = ensureConfigDraft(configWidget.value)
    if (configWidget.value.type === 'customShape') syncCustomShapeDefinitionFromRuntime()
    const nextConfig = cloneConfig(draft)
    store.updateWidget(widgetId, { config: nextConfig })
    syncConnectionsAfterPortConfig(widgetId, nextConfig)
  }
}

function closeConfig() {
  configWidgetId.value = null
}

function openWidgetDelete(widget) {
  deleteWidgetTarget.value = widget
}

function confirmWidgetDelete() {
  if (!deleteWidgetTarget.value) return
  const id = deleteWidgetTarget.value.id
  store.removeWidget(id)
  if (selectedId.value === id) selectedId.value = null
  selectedWidgetIds.value = selectedWidgetIds.value.filter(item => item !== id)
  if (configWidgetId.value === id) configWidgetId.value = null
  if (connectionDraftFrom.value === id) connectionDraftFrom.value = null
  deleteWidgetTarget.value = null
}

function resetLayout() {
  resetConfirmOpen.value = true
}

function confirmResetLayout() {
  store.resetLayout()
  selectedId.value = null
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
  connectionDraftFrom.value = null
  configWidgetId.value = null
  resetConfirmOpen.value = false
}

// ============ 项目管理 ============
const showProjectDialog = ref(false)
const projectDialogCreateOnOpen = ref(false)

function openProjectDialog() {
  projectDialogCreateOnOpen.value = false
  showProjectDialog.value = true
}

function closeProjectDialog() {
  showProjectDialog.value = false
  projectDialogCreateOnOpen.value = false
}

async function saveCurrentCanvas() {
  const current = projectStore.currentProject
  if (!current) {
    projectDialogCreateOnOpen.value = true
    showProjectDialog.value = true
    return
  }

  try {
    const updated = await projectStore.updateProject(current.id, {
      name: current.name,
      description: current.description || '',
      layout: store.layoutSnapshot
    })
    showConnectionNotice(`已更新保存“${updated.name}”`)
  } catch (error) {
    if (error?.code === 'PROJECT_NOT_FOUND') {
      projectStore.clearCurrentProject()
      projectDialogCreateOnOpen.value = true
      showProjectDialog.value = true
    }
    showConnectionNotice(error?.message || '保存项目失败')
  }
}

function applyProjectLayout(layout) {
  store.replaceLayout(layout)
  selectedId.value = null
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
  connectionDraftFrom.value = null
  configWidgetId.value = null
}
// ============ 项目管理 ============

const APPLICATION_ZOOM_MIN = 0.25
const APPLICATION_ZOOM_MAX = 4

function updateApplicationCanvasSize() {
  const canvas = applicationCanvasRef.value
  if (!canvas) return
  const width = Math.max(0, Math.round(canvas.clientWidth || 0))
  const height = Math.max(0, Math.round(canvas.clientHeight || 0))
  if (applicationCanvasSize.value.width !== width || applicationCanvasSize.value.height !== height) {
    applicationCanvasSize.value = { width, height }
  }
}

function getApplicationStageMetrics(zoom = applicationZoom.value) {
  const width = Math.ceil(applicationBounds.value.width * zoom)
  const height = Math.ceil(applicationBounds.value.height * zoom)
  const stageWidth = Math.max(width, applicationCanvasSize.value.width)
  const stageHeight = Math.max(height, applicationCanvasSize.value.height)
  return {
    width,
    height,
    stageWidth,
    stageHeight,
    offsetX: Math.max(0, (stageWidth - width) / 2),
    offsetY: Math.max(0, (stageHeight - height) / 2)
  }
}

function centerApplicationViewport() {
  const canvas = applicationCanvasRef.value
  if (!canvas) return
  updateApplicationCanvasSize()
  requestAnimationFrame(() => {
    const metrics = getApplicationStageMetrics()
    canvas.scrollLeft = Math.max(0, (metrics.stageWidth - canvas.clientWidth) / 2)
    canvas.scrollTop = Math.max(0, (metrics.stageHeight - canvas.clientHeight) / 2)
  })
}

function handleApplicationResize() {
  updateApplicationCanvasSize()
  if (applicationMode.value) centerApplicationViewport()
}

function setApplicationZoom(nextZoom, clientX = null, clientY = null) {
  const canvas = applicationCanvasRef.value
  const oldZoom = applicationZoom.value
  const next = Math.max(APPLICATION_ZOOM_MIN, Math.min(APPLICATION_ZOOM_MAX, Number(nextZoom) || 1))
  const keepCentered = !Number.isFinite(clientX) && !Number.isFinite(clientY)
  if (!canvas || Math.abs(next - oldZoom) < 0.0001) {
    applicationZoom.value = next
    applicationZoomInput.value = Math.round(next * 100)
    if (keepCentered) requestAnimationFrame(centerApplicationViewport)
    return
  }

  updateApplicationCanvasSize()
  if (keepCentered) {
    applicationZoom.value = Math.round(next * 10000) / 10000
    applicationZoomInput.value = Math.round(applicationZoom.value * 100)
    requestAnimationFrame(centerApplicationViewport)
    return
  }

  const rect = canvas.getBoundingClientRect()
  const anchorX = clientX - rect.left
  const anchorY = clientY - rect.top
  const oldMetrics = getApplicationStageMetrics(oldZoom)
  const contentX = (canvas.scrollLeft + anchorX - oldMetrics.offsetX) / oldZoom
  const contentY = (canvas.scrollTop + anchorY - oldMetrics.offsetY) / oldZoom
  applicationZoom.value = Math.round(next * 10000) / 10000
  applicationZoomInput.value = Math.round(applicationZoom.value * 100)

  requestAnimationFrame(() => {
    updateApplicationCanvasSize()
    const nextMetrics = getApplicationStageMetrics()
    canvas.scrollLeft = nextMetrics.offsetX + contentX * applicationZoom.value - anchorX
    canvas.scrollTop = nextMetrics.offsetY + contentY * applicationZoom.value - anchorY
  })
}

function handleApplicationWheel(event) {
  if (!event.ctrlKey) return
  event.preventDefault()
  const factor = Math.exp(-event.deltaY * 0.0015)
  setApplicationZoom(applicationZoom.value * factor, event.clientX, event.clientY)
}

function zoomApplicationBy(direction) {
  setApplicationZoom((applicationZoomPercent.value + (direction > 0 ? 1 : -1)) / 100)
}

function applyApplicationZoomInput() {
  const percent = Number(applicationZoomInput.value)
  if (!Number.isFinite(percent)) {
    applicationZoomInput.value = applicationZoomPercent.value
    return
  }
  setApplicationZoom(percent / 100)
}

function resetApplicationZoom() {
  setApplicationZoom(1)
  requestAnimationFrame(centerApplicationViewport)
}

function enterApplicationMode() {
  cancelWaypointConnection(false)
  closeContextMenu()
  selectedId.value = null
  selectedWidgetIds.value = []
  selectedConnectionId.value = null
  connectionDraftFrom.value = null
  connectionMode.value = false
  configWidgetId.value = null
  applicationZoom.value = 1
  applicationZoomInput.value = 100
  applicationMode.value = true
  requestAnimationFrame(() => {
    updateApplicationCanvasSize()
    centerApplicationViewport()
  })
}

function exitApplicationMode() {
  closeContextMenu()
  applicationMode.value = false
}

onMounted(() => {
  updateCanvasAreaSize()
  requestAnimationFrame(updateCanvasAreaSize)
  document.addEventListener('keydown', handleKeyDown)
  document.addEventListener('contextmenu', openPlatformContextMenu)
  document.addEventListener('mousedown', closeContextMenuFromOutside)
  window.addEventListener('resize', closeContextMenu)
  window.addEventListener('resize', updateCanvasAreaSize)
  window.addEventListener('resize', handleApplicationResize)
  window.addEventListener('blur', closeContextMenu)
})

onUnmounted(() => {
  cancelWaypointConnection(false)
  document.removeEventListener('keydown', handleKeyDown)
  document.removeEventListener('contextmenu', openPlatformContextMenu)
  document.removeEventListener('mousedown', closeContextMenuFromOutside)
  window.removeEventListener('resize', closeContextMenu)
  window.removeEventListener('resize', updateCanvasAreaSize)
  window.removeEventListener('resize', handleApplicationResize)
  window.removeEventListener('blur', closeContextMenu)
  document.removeEventListener('mousemove', handleLibraryResize)
  document.removeEventListener('mouseup', stopLibraryResize)
  document.removeEventListener('mousemove', handleBottomDockResize)
  document.removeEventListener('mouseup', stopBottomDockResize)
  if (wheelFrame) cancelAnimationFrame(wheelFrame)
  if (wheelIdleTimer) clearTimeout(wheelIdleTimer)
  if (panFrame) cancelAnimationFrame(panFrame)
  if (libraryResizeFrame) cancelAnimationFrame(libraryResizeFrame)
  if (bottomDockResizeFrame) cancelAnimationFrame(bottomDockResizeFrame)
})
</script>

<style scoped>
.canvas-page {
  display: flex; height: 100%;
  background: var(--bg-secondary);
}
.canvas-page.resizing-library,
.canvas-page.resizing-bottom-dock {
  user-select: none;
}
.canvas-page.resizing-library {
  cursor: col-resize;
}
.canvas-page.resizing-bottom-dock {
  cursor: row-resize;
}

/* 左侧功能块库 */
.library-pane {
  background: #f8fafc;
  border-right: 1px solid var(--border-light);
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
  overflow: hidden;
  min-width: 232px;
  max-width: min(460px, 48vw);
  box-shadow: 8px 0 18px rgba(15, 23, 42, 0.035);
}
.library-header {
  padding: 20px 20px 16px;
  border-bottom: 1px solid var(--border-light);
  background: #ffffff;
}
.library-header h3 {
  margin: 0;
  font-size: 18px;
  font-weight: var(--fw-bold);
  color: var(--text-primary);
  letter-spacing: 0;
}
.library-header p {
  margin: 7px 0 0;
  color: var(--text-muted);
  font-size: 13px;
  line-height: 1.5;
}
.library-search {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 10px 14px;
  background: #ffffff;
  border-bottom: 1px solid #e5ebf2;
}
.library-search input {
  flex: 1;
  min-width: 0;
  height: 34px;
  padding: 0 11px;
  border: 1px solid #d7e0e8;
  border-radius: 8px;
  background: #f8fafc;
  color: #1f2937;
  font-size: 13px;
  outline: none;
  transition: border-color 0.18s ease-out, box-shadow 0.18s ease-out, background 0.18s ease-out;
}
.library-search input:focus {
  background: #ffffff;
  border-color: rgba(31, 143, 90, 0.48);
  box-shadow: 0 0 0 3px rgba(31, 143, 90, 0.12);
}
.library-search button {
  height: 30px;
  padding: 0 9px;
  border: 1px solid #d7e0e8;
  border-radius: 7px;
  background: #ffffff;
  color: #667085;
  font-size: 13px;
  font-weight: 700;
  cursor: pointer;
  transition: background 0.18s ease-out, color 0.18s ease-out, border-color 0.18s ease-out;
}
.library-search button:hover {
  background: #eef5f2;
  border-color: rgba(31, 143, 90, 0.28);
  color: #176e45;
}
.library-import {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 6px;
  padding: 9px 14px;
  border-bottom: 1px solid #e5ebf2;
  background: #f4f8fb;
}
.library-import span { color: var(--text-tertiary); font-size: 11px; }
.hidden-file-input { display: none; }
.library-categories {
  flex: 1;
  overflow-y: auto;
  padding: 14px 14px 22px;
}
.library-empty {
  margin: 18px 6px;
  padding: 18px 10px;
  border-radius: 10px;
  background: #ffffff;
  border: 1px dashed #d7e0e8;
  color: #7b8794;
  font-size: 13px;
  font-weight: 700;
  text-align: center;
}
.lib-category {
  margin-bottom: 18px;
  border: 0;
  border-radius: 0;
  background: transparent;
  overflow: visible;
}
.cat-header {
  display: flex;
  align-items: center;
  gap: 9px;
  min-height: 48px;
  padding: 6px 8px;
  cursor: pointer;
  user-select: none;
  color: var(--text-secondary);
  border-radius: 10px;
  transition: background var(--transition-fast), color var(--transition-fast);
}
.cat-header:hover {
  background: #eef5f2;
  color: var(--text-primary);
}
.cat-toggle {
  width: 10px;
  color: #98a2b3;
  font-size: 13px;
  text-align: center;
  flex-shrink: 0;
}
.cat-icon {
  display: none;
  align-items: center;
  justify-content: center;
  width: 30px;
  height: 30px;
  border-radius: 9px;
  background: #eef7f2;
  border: 1px solid rgba(31, 143, 90, 0.18);
  color: #176e45;
  font-size: 14px;
  font-weight: var(--fw-bold);
  flex-shrink: 0;
}
.cat-text {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 4px;
}
.cat-name {
  color: #111827;
  font-size: 14px;
  font-weight: 750;
  line-height: 1.2;
}
.cat-desc {
  color: #667085;
  font-size: 13px;
  line-height: 1.35;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cat-count {
  min-width: 24px;
  height: 20px;
  padding: 0 7px;
  border-radius: 999px;
  background: #edf4f7;
  border: 1px solid #d9e5ec;
  color: #344054;
  font-size: 13px;
  font-weight: 700;
  line-height: 18px;
  text-align: center;
  flex-shrink: 0;
}
.cat-items {
  padding: 8px 0 0 20px;
  margin-left: 10px;
  border-left: 0;
  background: transparent;
}
.lib-subcategory {
  margin: 4px 0 14px;
  border-radius: 0;
  background: transparent;
  border: 0;
  overflow: visible;
}
.subcat-header {
  display: flex;
  align-items: center;
  gap: 8px;
  min-height: 38px;
  padding: 4px 6px;
  color: var(--text-tertiary);
  cursor: pointer;
  user-select: none;
  transition: background var(--transition-fast), color var(--transition-fast);
  border-radius: 8px;
}
.subcat-header:hover {
  background: #eef2f6;
  color: var(--text-secondary);
}
.subcat-toggle {
  width: 12px;
  color: var(--text-placeholder);
  font-size: 13px;
  text-align: center;
  flex-shrink: 0;
}
.subcat-text {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 3px;
}
.subcat-name {
  color: #344054;
  font-size: 13px;
  font-weight: 750;
  line-height: 1.2;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.subcat-desc {
  color: #7b8794;
  font-size: 13px;
  line-height: 1.25;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.subcat-count {
  min-width: 22px;
  height: 20px;
  padding: 0 7px;
  border-radius: 999px;
  background: #f1f5f9;
  border: 1px solid #d8e2ec;
  color: #475467;
  font-size: 13px;
  font-weight: 700;
  line-height: 18px;
  text-align: center;
}
.subcat-items {
  padding: 4px 2px 2px 20px;
  border-top: 0;
}
.lib-item {
  display: flex;
  align-items: center;
  gap: 10px;
  min-height: 36px;
  padding: 0 11px;
  margin: 3px 0;
  border-left: 3px solid;
  background: transparent;
  border-radius: 8px;
  cursor: grab;
  user-select: none;
  transition: background var(--transition-fast), color var(--transition-fast), border-color var(--transition-fast);
  font-size: 13px;
  color: #334155;
  border-top: 0;
  border-right: 0;
  border-bottom: 0;
  box-shadow: none;
}
.lib-item:hover {
  background: #e9f1f5;
  color: #111827;
}
.lib-item:active {
  cursor: grabbing;
  opacity: 0.84;
}
.lib-item-icon {
  display: none;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
  border-radius: 7px;
  background: #ffffff;
  border: 1px solid #d8e2ec;
  font-size: 13px;
  font-weight: var(--fw-bold);
  text-align: center;
  flex-shrink: 0;
}
.lib-item-name {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-weight: 700;
}
.lib-item-delete {
  width: 22px;
  height: 22px;
  flex: 0 0 auto;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  border: 1px solid transparent;
  border-radius: 5px;
  background: transparent;
  color: var(--text-placeholder);
  font-size: 16px;
  line-height: 1;
}
.lib-item-delete:hover { border-color: var(--danger-border); background: var(--danger-bg); color: var(--danger-text); }

.library-preview-popover {
  position: fixed;
  z-index: 1200;
  width: 246px;
  padding: 12px;
  pointer-events: none;
  background: rgba(255, 255, 255, 0.96);
  border: 1px solid rgba(205, 216, 226, 0.9);
  border-radius: 12px;
  box-shadow: 0 16px 36px rgba(15, 23, 42, 0.14);
  backdrop-filter: blur(8px);
}
.library-preview-title {
  margin-bottom: 10px;
  color: #1f2937;
  font-size: 14px;
  font-weight: 800;
  line-height: 1.2;
}
.library-preview-stage {
  display: flex;
  align-items: stretch;
  justify-content: stretch;
  margin: 0 auto;
  overflow: hidden;
  background:
    linear-gradient(rgba(100, 116, 139, 0.08) 1px, transparent 1px),
    linear-gradient(90deg, rgba(100, 116, 139, 0.08) 1px, transparent 1px),
    #f8fafc;
  background-size: 16px 16px;
  border: 1px solid #e2e8f0;
  border-radius: 10px;
}

.pane-resizer {
  position: relative;
  width: 7px;
  cursor: col-resize;
  background: #f1f5f9;
  border-right: 1px solid #e2e8f0;
  flex-shrink: 0;
  transition: background 0.16s ease-out;
}
.pane-resizer::after {
  content: '';
  position: absolute;
  top: 50%;
  left: 3px;
  width: 1px;
  height: 44px;
  transform: translateY(-50%);
  border-radius: 999px;
  background: #c6d1de;
}
.pane-resizer:hover,
.canvas-page.resizing-library .pane-resizer {
  background: #e6f3ee;
}
.pane-resizer:hover::after,
.canvas-page.resizing-library .pane-resizer::after {
  background: #1f8f5a;
}

/* 画布 */
.canvas-container { flex: 1; display: flex; flex-direction: column; background: var(--bg-canvas); min-width: 0; }
.canvas-and-dock { flex: 1; display: flex; flex-direction: column; min-height: 0; }
.canvas-toolbar-mini { display: flex; align-items: center; gap: 8px; min-height: 46px; padding: 6px 14px; border-bottom: 1px solid var(--border-light); background: var(--bg-primary); flex-shrink: 0; box-shadow: var(--shadow-xs); }
.canvas-info { margin-left: auto; font-size: var(--fs-sm); color: var(--text-placeholder); }
.canvas-area {
  flex: 1; position: relative; overflow: hidden; min-height: 300px;
  background-color: var(--bg-canvas);
  background-image:
    linear-gradient(rgba(100,116,139,0.10) 1px, transparent 1px),
    linear-gradient(90deg, rgba(100,116,139,0.10) 1px, transparent 1px);
  background-size: 20px 20px;
  background-position: -1px -1px;
}
.canvas-area.panning { cursor: grabbing; }
.canvas-viewport {
  position: absolute; top: 0; left: 0;
  transform-origin: 0 0;
  min-width: 1px;
  min-height: 1px;
}
.canvas-spacer {
  min-width: 1px;
  min-height: 1px;
  pointer-events: none;
  background: transparent;
}
.connection-hint {
  display: inline-flex;
  align-items: center;
  min-height: 26px;
  padding: 0 9px;
  border-radius: var(--radius-full);
  background: rgba(31, 143, 90, 0.12);
  color: #176e45;
  font-size: var(--fs-sm);
  font-weight: var(--fw-semibold);
}
.connection-notice {
  display: inline-flex;
  align-items: center;
  min-height: 26px;
  padding: 0 10px;
  border-radius: var(--radius-full);
  background: rgba(245, 158, 11, 0.13);
  color: #92400e;
  font-size: var(--fs-sm);
  font-weight: var(--fw-semibold);
}
.connection-layer {
  position: absolute;
  inset: 0;
  min-width: 1px;
  min-height: 1px;
  overflow: visible;
  pointer-events: auto;
  z-index: 1;
  shape-rendering: geometricPrecision;
  text-rendering: geometricPrecision;
}
.area-selection-box {
  position: absolute;
  z-index: 900;
  pointer-events: none;
  border: 1px solid rgba(79, 111, 184, 0.92);
  background: rgba(79, 111, 184, 0.13);
  box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.38);
}
.area-selection-toolbar {
  position: absolute;
  z-index: 1200;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  min-height: 34px;
  padding: 4px 6px 4px 10px;
  border: 1px solid rgba(79, 111, 184, 0.28);
  border-radius: var(--radius-md);
  background: rgba(255, 255, 255, 0.94);
  box-shadow: var(--shadow-md);
  color: var(--text-secondary);
  font-size: var(--fs-sm);
  font-weight: var(--fw-semibold);
  white-space: nowrap;
}
.frame-box-hit-proxy {
  position: absolute;
  z-index: 950;
  pointer-events: none;
}
.frame-hit-edge {
  position: absolute;
  pointer-events: auto;
  background: transparent;
}
.frame-hit-edge.hit-top {
  left: -8px;
  right: -8px;
  top: -8px;
  height: 16px;
  cursor: pointer;
}
.frame-hit-edge.hit-bottom {
  left: -8px;
  right: -8px;
  bottom: -8px;
  height: 16px;
  cursor: pointer;
}
.frame-hit-edge.hit-left {
  left: -8px;
  top: -8px;
  bottom: -8px;
  width: 16px;
  cursor: pointer;
}
.frame-hit-edge.hit-right {
  right: -8px;
  top: -8px;
  bottom: -8px;
  width: 16px;
  cursor: pointer;
}
.connection-group {
  pointer-events: auto;
}
.connection-hit {
  fill: none;
  stroke: transparent;
  stroke-width: 16;
  stroke-linecap: butt;
  stroke-linejoin: miter;
  cursor: pointer;
  pointer-events: stroke;
}
.connection-outline {
  fill: none;
  stroke: rgba(20, 28, 36, 0.62);
  stroke-linecap: round;
  stroke-linejoin: miter;
  vector-effect: non-scaling-stroke;
  pointer-events: none;
}
.connection-line {
  fill: none;
  stroke-linecap: round;
  stroke-linejoin: miter;
  vector-effect: non-scaling-stroke;
  filter: none;
  pointer-events: stroke;
}
.connection-line.selected {
  stroke: #f59e0b;
  stroke-width: 5;
  filter: none;
}
.connection-handle {
  fill: #ffffff;
  stroke: #f59e0b;
  stroke-width: 3;
  cursor: grab;
  pointer-events: all;
  filter: drop-shadow(0 2px 5px rgba(15, 23, 42, 0.24));
}
.connection-handle:active {
  cursor: grabbing;
}
.connection-waypoint-handle {
  fill: #ffffff;
  stroke: #2563eb;
  stroke-width: 3;
  cursor: move;
  pointer-events: all;
  vector-effect: non-scaling-stroke;
  filter: drop-shadow(0 2px 5px rgba(15, 23, 42, 0.24));
}
.connection-waypoint-handle.alignment-hot,
.connection-handle.alignment-hot {
  fill: rgba(240, 253, 250, 0.96);
  stroke: #00d4ff;
  stroke-width: 4;
  filter: drop-shadow(0 0 8px rgba(0, 212, 255, 0.75));
}
.connection-insert-handle {
  fill: #16a34a;
  stroke: #ffffff;
  stroke-width: 2;
  cursor: move;
  pointer-events: all;
  vector-effect: non-scaling-stroke;
  opacity: 0.9;
  filter: drop-shadow(0 1px 4px rgba(15, 23, 42, 0.22));
}
.connection-insert-handle.segment-horizontal {
  cursor: ns-resize;
}
.connection-insert-handle.segment-vertical {
  cursor: ew-resize;
}
.connection-insert-handle.alignment-hot {
  fill: #0ea5e9;
  stroke: #ffffff;
  stroke-width: 3;
  filter: drop-shadow(0 0 7px rgba(14, 165, 233, 0.72));
}
.connection-insert-handle:hover {
  fill: #22c55e;
  transform-box: fill-box;
  transform-origin: center;
  transform: scale(1.22);
}
.orthogonal-guide-line {
  stroke: rgba(14, 165, 233, 0.62);
  stroke-width: 1.5;
  stroke-dasharray: 9 7;
  vector-effect: non-scaling-stroke;
  pointer-events: none;
}
.rubber-band-line {
  fill: none;
  stroke: rgba(37, 99, 235, 0.86);
  stroke-width: 2.5;
  stroke-dasharray: 8 6;
  stroke-linecap: butt;
  vector-effect: non-scaling-stroke;
  pointer-events: none;
}
.rubber-band-line.alignment-hot {
  stroke: #22c55e;
  filter: drop-shadow(0 0 6px rgba(34, 197, 94, 0.45));
}
.rubber-band-line.manual-trace {
  stroke: #7c3aed;
  stroke-width: 3;
  stroke-dasharray: 10 5;
  filter: drop-shadow(0 0 5px rgba(124, 58, 237, 0.32));
}
.rubber-band-point {
  fill: rgba(255, 255, 255, 0.95);
  stroke: #2563eb;
  stroke-width: 3;
  pointer-events: none;
  vector-effect: non-scaling-stroke;
}
.rubber-band-point.alignment-hot {
  fill: rgba(240, 253, 250, 0.96);
  stroke: #22c55e;
  stroke-width: 4;
  filter: drop-shadow(0 0 9px rgba(34, 197, 94, 0.7));
}
.rubber-band-waypoint {
  fill: #7c3aed;
  stroke: #ffffff;
  stroke-width: 2;
  pointer-events: none;
  vector-effect: non-scaling-stroke;
  filter: drop-shadow(0 0 4px rgba(124, 58, 237, 0.45));
}
.canvas-viewport :deep(.widget-wrapper) {
  z-index: 2;
}

/* 右侧配置面板 */
.config-pane { width: 292px; background: var(--bg-primary); border-left: 1px solid var(--border-light); display: flex; flex-direction: column; flex-shrink: 0; overflow-y: auto; box-shadow: -8px 0 18px rgba(16,24,40,0.04); }
.config-header { display: flex; align-items: center; justify-content: space-between; gap: 10px; padding: 14px 16px; border-bottom: 1px solid var(--border-light); background: var(--gradient-pane); }
.config-header h3 { font-size: var(--fs-sm); font-weight: var(--fw-bold); color: var(--text-primary); }
.config-instance { margin-top: 2px; font-size: 13px; color: var(--text-placeholder); font-family: var(--font-mono); }
.config-body { padding: 16px; flex: 1; overflow-y: auto; }
.config-body .form-group { margin-bottom: var(--sp-sm); }
.form-row.two-col {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
}
.form-row.two-col .form-group {
  min-width: 0;
}
.label-style-toggles {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 6px;
  margin-bottom: var(--sp-sm);
}
.label-style-toggles.two-items {
  grid-template-columns: repeat(2, 1fr);
}
.rotation-action-row {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 8px;
}
.label-style-toggles label {
  display: flex;
  align-items: center;
  gap: 5px;
  min-height: 32px;
  padding: 0 7px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-sm);
  background: var(--surface-muted);
  color: var(--text-secondary);
  font-size: var(--fs-sm);
}
.label-style-toggles input {
  width: 14px;
  height: 14px;
  min-height: 0;
  accent-color: var(--accent);
}
.config-section {
  margin: 12px 0;
  padding: 12px 0 0;
  border-top: 1px solid var(--border-light);
}
.section-title {
  margin: 0 0 9px;
  color: var(--text-primary);
  font-size: var(--fs-sm);
  font-weight: 800;
}
.inline-check {
  display: flex;
  align-items: center;
  gap: 7px;
  min-height: 30px;
  margin-bottom: 8px;
  color: var(--text-secondary);
  font-size: var(--fs-sm);
  font-weight: 700;
}
.inline-check input {
  width: 14px;
  height: 14px;
  accent-color: var(--accent);
}
.inline-check.name-toggle {
  margin-bottom: var(--sp-sm);
}
.mutual-exclusion-config {
  padding: 10px;
  border: 1px solid #c8d7fb;
  border-radius: var(--radius-sm);
  background: #f7f9ff;
}
.mutual-exclusion-config .section-title {
  color: #3158a4;
}
.mutual-exclusion-help {
  margin: -1px 0 9px;
  color: var(--text-secondary);
  font-size: 12px;
  line-height: 1.45;
}
.mutual-exclusion-list {
  display: flex;
  flex-direction: column;
  gap: 6px;
  max-height: 190px;
  overflow-y: auto;
}
.mutual-exclusion-item {
  display: flex;
  align-items: flex-start;
  gap: 7px;
  padding: 7px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-sm);
  background: var(--bg-primary);
  cursor: pointer;
}
.mutual-exclusion-item.selected {
  border-color: #6387da;
  background: #eef3ff;
}
.mutual-exclusion-item.disabled {
  cursor: not-allowed;
  opacity: 0.66;
}
.mutual-exclusion-item input {
  width: 14px;
  height: 14px;
  margin-top: 2px;
  accent-color: var(--accent);
}
.mutual-exclusion-item-text {
  display: flex;
  min-width: 0;
  flex: 1;
  flex-direction: column;
  gap: 2px;
}
.mutual-exclusion-item-text strong,
.mutual-exclusion-item-text small {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.mutual-exclusion-item-text strong {
  color: var(--text-primary);
  font-size: 12px;
}
.mutual-exclusion-item-text small {
  color: var(--text-secondary);
  font-size: 11px;
}
.mutual-exclusion-warning {
  color: var(--danger-text, #b42318) !important;
}
.port-config-list {
  display: flex;
  flex-direction: column;
  gap: 7px;
}
.port-config-row {
  display: grid;
  grid-template-columns: 54px minmax(0, 1fr) 42px;
  align-items: center;
  gap: 8px;
  min-height: 34px;
  padding: 6px 8px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-sm);
  background: var(--surface-muted);
}
.port-config-row label {
  display: flex;
  align-items: center;
  gap: 5px;
  color: var(--text-secondary);
  font-size: var(--fs-sm);
  font-weight: 700;
}
.port-config-row input[type="range"] {
  min-width: 0;
}
.port-config-row span {
  color: var(--text-tertiary);
  font-size: 13px;
  font-family: var(--font-mono);
  text-align: right;
}

/* 变量选择 checkbox */
.var-binding-search { display: flex; align-items: center; gap: 8px; margin: 6px 0 8px; }
.var-binding-search input { min-width: 0; flex: 1; height: 32px; padding: 0 9px; font-size: var(--fs-sm); }
.var-binding-search span { flex: 0 0 auto; min-width: 42px; color: var(--text-placeholder); font-family: var(--font-mono); font-size: 12px; text-align: right; }
.var-checkbox-list { display: flex; flex-direction: column; gap: 8px; max-height: 260px; overflow-y: auto; border: 1px solid var(--border-light); border-radius: var(--radius-md); padding: 8px; background: var(--surface-muted); }
.var-select-group { display: flex; flex-direction: column; gap: 4px; }
.var-select-group-title { display: flex; align-items: center; justify-content: space-between; width: 100%; min-height: 28px; padding: 0 6px; color: var(--text-secondary); font-size: var(--fs-sm); font-weight: 800; border-radius: var(--radius-sm); text-align: left; }
.var-select-group-title:hover { background: var(--bg-hover); }
.var-select-group-name { display: inline-flex; align-items: center; min-width: 0; gap: 5px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.var-select-group-name b { width: 11px; color: var(--text-placeholder); font-size: 11px; }
.var-select-group-items { display: flex; flex-direction: column; gap: 2px; }
.var-select-group-title span { min-width: 20px; height: 20px; padding: 0 6px; border-radius: var(--radius-full); background: #edf4f7; border: 1px solid #d9e5ec; color: var(--text-tertiary); font-family: var(--font-mono); font-size: 12px; line-height: 18px; text-align: center; }
.var-checkbox { display: grid; grid-template-columns: 18px minmax(0,1fr) auto; align-items: center; gap: 8px; min-height: 32px; padding: 4px 6px; border-radius: var(--radius-sm); cursor: pointer; font-size: var(--fs-sm); transition: background var(--transition-fast); }
.var-checkbox input[type="checkbox"] { margin: 0; width: 15px; height: 15px; accent-color: var(--accent); }
.var-checkbox:hover { background: var(--bg-hover); }
.var-checkbox-name { padding-left: 6px; border-left: 3px solid; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.var-checkbox-meta { font-size: 13px; color: var(--text-placeholder); white-space: nowrap; }

/* 变量信息卡片 */
.var-info-card { margin-top: 8px; padding: 10px 12px; background: var(--surface-muted); border-radius: var(--radius-md); border: 1px solid var(--border-light); font-size: var(--fs-sm); }
.var-info-row { display: flex; justify-content: space-between; align-items: center; padding: 2px 0; color: var(--text-tertiary); }
/* 应用模式 */
.application-overlay {
  position: fixed;
  inset: 0;
  z-index: 1500;
  display: flex;
  flex-direction: column;
  background: var(--bg-secondary);
}
.application-toolbar {
  height: 46px;
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto minmax(0, 1fr);
  align-items: center;
  gap: 12px;
  padding: 0 16px;
  border-bottom: 1px solid var(--border-light);
  background: var(--bg-primary);
  box-shadow: var(--shadow-xs);
  flex-shrink: 0;
}
.application-toolbar > .btn {
  justify-self: end;
}
.application-title {
  font-size: var(--fs-sm);
  font-weight: var(--fw-semibold);
  color: var(--text-secondary);
}
.application-zoom-controls {
  display: inline-grid;
  grid-template-columns: 30px 66px 30px 44px;
  align-items: center;
  gap: 4px;
  padding: 3px;
  border: 1px solid var(--border-light);
  border-radius: 7px;
  background: var(--surface-muted);
}
.application-zoom-button,
.application-zoom-value,
.application-zoom-reset {
  height: 28px;
  padding: 0;
  border: 0;
  border-radius: 5px;
  background: transparent;
  color: var(--text-secondary);
  font: inherit;
  font-size: 15px;
  cursor: pointer;
  transition: background 0.16s ease-out, color 0.16s ease-out;
}
.application-zoom-value {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 1px;
  font-size: 13px;
  font-weight: 700;
  cursor: text;
}
.application-zoom-value input {
  width: 43px;
  min-width: 0;
  height: 25px;
  padding: 0 2px;
  border: 0;
  outline: 0;
  text-align: right;
  color: inherit;
  background: transparent;
  font: inherit;
}
.application-zoom-reset { font-size: 11px; font-weight: 700; }
.application-zoom-button:hover,
.application-zoom-reset:hover {
  background: var(--bg-hover);
  color: var(--accent);
}
.application-canvas {
  flex: 1;
  position: relative;
  overflow: auto;
  background: var(--bg-canvas);
}
.application-stage {
  position: relative;
  min-width: 1px;
  min-height: 1px;
}
.application-content {
  position: absolute;
  left: 0;
  top: 0;
  min-width: 1px;
  min-height: 1px;
}
.app-connection-layer {
  right: auto;
  bottom: auto;
}
.application-content :deep(.widget-wrapper) {
  z-index: 2;
}
.application-empty {
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-placeholder);
  font-size: var(--fs-sm);
}

.platform-context-menu {
  position: fixed;
  z-index: 3200;
  width: 228px;
  padding: 6px;
  border: 1px solid rgba(152, 162, 179, 0.32);
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.98);
  box-shadow: 0 14px 34px rgba(16, 24, 40, 0.18), 0 3px 8px rgba(16, 24, 40, 0.08);
  backdrop-filter: blur(10px);
  user-select: none;
}
.context-menu-title {
  min-height: 30px;
  display: flex;
  align-items: center;
  padding: 4px 10px 6px;
  color: var(--text-tertiary);
  font-size: 13px;
  font-weight: 700;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.context-menu-item {
  width: 100%;
  min-height: 36px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 14px;
  padding: 7px 10px;
  border: 0;
  border-radius: 5px;
  background: transparent;
  color: var(--text-primary);
  font: inherit;
  font-size: 13px;
  text-align: left;
  cursor: pointer;
  transition: background 0.16s ease-out, color 0.16s ease-out;
}
.context-menu-item:hover:not(:disabled) {
  background: var(--bg-hover);
  color: var(--accent);
}
.context-menu-item:focus-visible {
  outline: 2px solid rgba(79, 111, 184, 0.34);
  outline-offset: -2px;
}
.context-menu-item:disabled {
  color: var(--text-placeholder);
  cursor: default;
  opacity: 0.55;
}
.context-menu-item.danger {
  color: #b4232d;
}
.context-menu-item.danger:hover:not(:disabled) {
  color: #981b24;
  background: rgba(194, 65, 75, 0.09);
}
.context-menu-item kbd {
  flex-shrink: 0;
  padding: 0;
  border: 0;
  background: transparent;
  color: var(--text-placeholder);
  font-family: var(--font-family);
  font-size: 13px;
  font-weight: 500;
}
.context-menu-separator {
  height: 1px;
  margin: 5px 6px;
  background: var(--border-light);
}

@media (max-width: 820px) {
  .library-pane { max-width: 52vw; }
  .config-pane { width: 260px; }
  .canvas-toolbar-mini { overflow-x: auto; }
}
</style>
