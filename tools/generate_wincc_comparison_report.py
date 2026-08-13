import html
import os
import zipfile
from datetime import datetime


OUT_DIR = os.path.join("docs")
OUT_FILE = os.path.join(OUT_DIR, "西门子WinCC与自研监控平台架构功能对比报告.docx")

NS_W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
NS_R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"


def x(text):
    return html.escape(str(text), quote=False)


def attrs(**kwargs):
    return " ".join(f'{k}="{x(v)}"' for k, v in kwargs.items() if v is not None)


def rpr(bold=False, italic=False, size=None, color=None, font="Microsoft YaHei"):
    parts = [
        f'<w:rFonts w:ascii="{font}" w:hAnsi="{font}" w:eastAsia="{font}"/>',
    ]
    if bold:
        parts.append("<w:b/>")
        parts.append("<w:bCs/>")
    if italic:
        parts.append("<w:i/>")
    if color:
        parts.append(f'<w:color w:val="{color}"/>')
    if size:
        half = int(size * 2)
        parts.append(f'<w:sz w:val="{half}"/>')
        parts.append(f'<w:szCs w:val="{half}"/>')
    return "<w:rPr>" + "".join(parts) + "</w:rPr>"


def p(text="", style=None, align=None, bold=False, italic=False, size=None, color=None, spacing_after=120, before=0):
    ppr = []
    if style:
        ppr.append(f'<w:pStyle w:val="{style}"/>')
    if align:
        ppr.append(f'<w:jc w:val="{align}"/>')
    if spacing_after is not None or before:
        ppr.append(f'<w:spacing w:after="{spacing_after or 0}" w:before="{before or 0}" w:line="300" w:lineRule="auto"/>')
    ppr_xml = "<w:pPr>" + "".join(ppr) + "</w:pPr>" if ppr else ""
    lines = str(text).split("\n")
    runs = []
    for idx, line in enumerate(lines):
        if idx:
            runs.append("<w:r><w:br/></w:r>")
        preserve = " xml:space=\"preserve\"" if line.startswith(" ") or line.endswith(" ") else ""
        runs.append(f"<w:r>{rpr(bold=bold, italic=italic, size=size, color=color)}<w:t{preserve}>{x(line)}</w:t></w:r>")
    return f"<w:p>{ppr_xml}{''.join(runs)}</w:p>"


def page_break():
    return '<w:p><w:r><w:br w:type="page"/></w:r></w:p>'


def cell(text, width=None, header=False):
    shading = '<w:shd w:val="clear" w:color="auto" w:fill="EAF1F8"/>' if header else ""
    tcw = f'<w:tcW w:w="{width}" w:type="dxa"/>' if width else ""
    v_align = '<w:vAlign w:val="center"/>'
    tcpr = f"<w:tcPr>{tcw}{v_align}{shading}</w:tcPr>"
    paras = []
    if isinstance(text, (list, tuple)):
        for item in text:
            paras.append(p(item, style="TableText", bold=header, spacing_after=60))
    else:
        paras.append(p(text, style="TableText", bold=header, spacing_after=60))
    return f"<w:tc>{tcpr}{''.join(paras)}</w:tc>"


def table(rows, widths=None, caption=None):
    widths = widths or []
    out = []
    if caption:
        out.append(p(caption, style="Caption", bold=True, color="44546A", spacing_after=80))
    out.append(
        '<w:tbl><w:tblPr>'
        '<w:tblStyle w:val="TableGrid"/>'
        '<w:tblW w:w="0" w:type="auto"/>'
        '<w:tblCellMar>'
        '<w:top w:w="90" w:type="dxa"/><w:left w:w="90" w:type="dxa"/>'
        '<w:bottom w:w="90" w:type="dxa"/><w:right w:w="90" w:type="dxa"/>'
        '</w:tblCellMar>'
        '<w:tblLook w:firstRow="1" w:lastRow="0" w:firstColumn="0" w:lastColumn="0" '
        'w:noHBand="0" w:noVBand="1"/>'
        '</w:tblPr><w:tblGrid>'
    )
    if widths:
        for w in widths:
            out.append(f'<w:gridCol w:w="{w}"/>')
    out.append("</w:tblGrid>")
    for ridx, row in enumerate(rows):
        out.append("<w:tr>")
        for cidx, val in enumerate(row):
            out.append(cell(val, widths[cidx] if cidx < len(widths) else None, header=(ridx == 0)))
        out.append("</w:tr>")
    out.append("</w:tbl>")
    out.append(p("", spacing_after=100))
    return "".join(out)


def diagram(title, lines):
    body = "\n".join(lines)
    return (
        p(title, style="Caption", bold=True, color="44546A", spacing_after=80)
        + p(body, style="Code", size=9, spacing_after=140)
    )


def h1(text):
    return p(text, style="Heading1", spacing_after=160, before=180)


def h2(text):
    return p(text, style="Heading2", spacing_after=120, before=120)


def bullet(text):
    return p("• " + text, style="Normal", spacing_after=80)


def styles_xml():
    return f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="{NS_W}">
  <w:style w:type="paragraph" w:default="1" w:styleId="Normal">
    <w:name w:val="Normal"/>
    <w:qFormat/>
    <w:pPr><w:spacing w:after="120" w:line="300" w:lineRule="auto"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Microsoft YaHei" w:hAnsi="Microsoft YaHei" w:eastAsia="Microsoft YaHei"/><w:sz w:val="21"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Title">
    <w:name w:val="Title"/><w:basedOn w:val="Normal"/><w:qFormat/>
    <w:pPr><w:jc w:val="center"/><w:spacing w:after="220"/></w:pPr>
    <w:rPr><w:b/><w:rFonts w:ascii="Microsoft YaHei" w:hAnsi="Microsoft YaHei" w:eastAsia="Microsoft YaHei"/><w:sz w:val="44"/><w:color w:val="1F4E79"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Subtitle">
    <w:name w:val="Subtitle"/><w:basedOn w:val="Normal"/><w:qFormat/>
    <w:pPr><w:jc w:val="center"/><w:spacing w:after="160"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Microsoft YaHei" w:hAnsi="Microsoft YaHei" w:eastAsia="Microsoft YaHei"/><w:sz w:val="24"/><w:color w:val="666666"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading1">
    <w:name w:val="heading 1"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:qFormat/>
    <w:pPr><w:outlineLvl w:val="0"/><w:spacing w:before="240" w:after="160"/></w:pPr>
    <w:rPr><w:b/><w:rFonts w:ascii="Microsoft YaHei" w:hAnsi="Microsoft YaHei" w:eastAsia="Microsoft YaHei"/><w:sz w:val="30"/><w:color w:val="1F4E79"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading2">
    <w:name w:val="heading 2"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:qFormat/>
    <w:pPr><w:outlineLvl w:val="1"/><w:spacing w:before="160" w:after="120"/></w:pPr>
    <w:rPr><w:b/><w:rFonts w:ascii="Microsoft YaHei" w:hAnsi="Microsoft YaHei" w:eastAsia="Microsoft YaHei"/><w:sz w:val="25"/><w:color w:val="2F5597"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Caption">
    <w:name w:val="Caption"/><w:basedOn w:val="Normal"/><w:qFormat/>
    <w:pPr><w:spacing w:before="80" w:after="80"/></w:pPr>
    <w:rPr><w:b/><w:rFonts w:ascii="Microsoft YaHei" w:hAnsi="Microsoft YaHei" w:eastAsia="Microsoft YaHei"/><w:sz w:val="20"/><w:color w:val="44546A"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Code">
    <w:name w:val="Code"/><w:basedOn w:val="Normal"/>
    <w:pPr><w:spacing w:after="140" w:line="240" w:lineRule="auto"/><w:ind w:left="180"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Consolas" w:hAnsi="Consolas" w:eastAsia="Microsoft YaHei"/><w:sz w:val="18"/><w:color w:val="333333"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="TableText">
    <w:name w:val="Table Text"/><w:basedOn w:val="Normal"/>
    <w:pPr><w:spacing w:after="60" w:line="260" w:lineRule="auto"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Microsoft YaHei" w:hAnsi="Microsoft YaHei" w:eastAsia="Microsoft YaHei"/><w:sz w:val="18"/></w:rPr>
  </w:style>
  <w:style w:type="table" w:styleId="TableGrid">
    <w:name w:val="Table Grid"/><w:basedOn w:val="TableNormal"/><w:uiPriority w:val="59"/><w:qFormat/>
    <w:tblPr><w:tblBorders>
      <w:top w:val="single" w:sz="6" w:space="0" w:color="D9E2F3"/>
      <w:left w:val="single" w:sz="6" w:space="0" w:color="D9E2F3"/>
      <w:bottom w:val="single" w:sz="6" w:space="0" w:color="D9E2F3"/>
      <w:right w:val="single" w:sz="6" w:space="0" w:color="D9E2F3"/>
      <w:insideH w:val="single" w:sz="6" w:space="0" w:color="D9E2F3"/>
      <w:insideV w:val="single" w:sz="6" w:space="0" w:color="D9E2F3"/>
    </w:tblBorders></w:tblPr>
  </w:style>
</w:styles>'''


def content_types_xml():
    return '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
  <Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>
  <Override PartName="/word/settings.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml"/>
  <Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
  <Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>
</Types>'''


def rels_xml():
    return '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/>
  <Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/>
</Relationships>'''


def doc_rels_xml():
    return '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings" Target="settings.xml"/>
</Relationships>'''


def settings_xml():
    return f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:settings xmlns:w="{NS_W}">
  <w:defaultTabStop w:val="420"/>
  <w:characterSpacingControl w:val="doNotCompress"/>
</w:settings>'''


def core_xml():
    now = datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")
    return f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:dcterms="http://purl.org/dc/terms/"
  xmlns:dcmitype="http://purl.org/dc/dcmitype/"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <dc:title>西门子WinCC与自研监控平台架构功能对比报告</dc:title>
  <dc:creator>Codex</dc:creator>
  <cp:lastModifiedBy>Codex</cp:lastModifiedBy>
  <dcterms:created xsi:type="dcterms:W3CDTF">{now}</dcterms:created>
  <dcterms:modified xsi:type="dcterms:W3CDTF">{now}</dcterms:modified>
</cp:coreProperties>'''


def app_xml():
    return '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties"
  xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">
  <Application>Codex OpenXML Generator</Application>
  <DocSecurity>0</DocSecurity>
  <ScaleCrop>false</ScaleCrop>
  <Company>NEU</Company>
</Properties>'''


def make_document_xml():
    parts = []
    parts.append(f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="{NS_W}" xmlns:r="{NS_R}">
<w:body>''')

    parts.append(p("西门子 WinCC 监控平台与自研监控平台", style="Title"))
    parts.append(p("架构与功能对比报告", style="Title", size=22))
    parts.append(p("面向工业监控 / SCADA / Web 组态运行环境", style="Subtitle"))
    parts.append(p("版本：V1.0    日期：2026-06-17    编制：Codex", align="center", color="666666", spacing_after=260))
    parts.append(table([
        ["项目", "内容"],
        ["对比对象", "Siemens SIMATIC WinCC Unified PC / WinCC V8；自研 MonitoringPlatform v1.0.0"],
        ["报告目的", "从架构、开发平台、应用平台、数据库、通信、组态画面、报警、历史趋势、部署、安全等维度评估差异。"],
        ["资料依据", "Siemens 官方公开资料；当前项目源码、数据库脚本、运行包文件结构。"],
        ["适用边界", "WinCC 具体能力与授权、版本、选件相关；自研平台能力以当前代码实现为准。"]
    ], [2800, 6200], "表 1 报告范围与依据"))

    parts.append(h1("一、结论摘要"))
    parts.append(p("总体判断：WinCC 是成熟工业 SCADA/HMI 平台，优势在工程体系、协议生态、归档报警、权限审计、冗余扩展、长期运维与工业安全；自研平台是轻量化、可定制、Web 化的监控运行平台，优势在源码可控、部署包小、功能贴合当前实验/项目流程、MySQL 宽表直观、与 OPC UA/UDP/JSON 数据源集成灵活。"))
    parts.append(p("现阶段自研平台适合用于实验室、教学验证、小型监控项目、快速原型和定制化展示；如果要替代 WinCC 用于连续生产现场，还需要补齐安全登录、用户权限、审计追踪、冗余容错、驱动生态、工程版本管理、报警生命周期、报表与备份恢复等能力。"))
    parts.append(table([
        ["维度", "WinCC 综合定位", "自研平台综合定位", "结论"],
        ["产品成熟度", "商业级工业 SCADA，长期迭代，面向生产系统。", "项目级自研运行平台，核心采集/显示/存储链路已具备。", "WinCC 更成熟；自研平台更轻量灵活。"],
        ["架构开放度", "提供 OPC UA、Web、REST/MQTT/GraphQL 等开放接口，受授权与平台体系约束。", "源码和数据结构完全可控，REST/WebSocket/MySQL 可直接扩展。", "自研平台二次开发成本低。"],
        ["部署复杂度", "需要工程软件、运行时、授权、系统规划。", "可打包为单个运行目录和 exe，前端嵌入后端。", "自研平台部署更轻，但生产保障不足。"],
        ["工业可靠性", "支持大规模客户端、服务器、冗余、审计、归档与安全能力。", "当前是单机后端 + MySQL + Web 前端，云隧道可选。", "生产级可靠性仍是差距。"],
        ["功能匹配度", "功能完整，覆盖 SCADA 全生命周期。", "已覆盖变量、实时、历史、趋势、报警、日志、组态画布、通信资源、项目管理。", "核心链路已成型，缺高级工程能力。"]
    ], [1500, 2900, 2900, 1700], "表 2 总体定位对比"))

    parts.append(h1("二、平台架构对比"))
    parts.append(p("WinCC 的典型架构是工程软件、运行时、工业通信、归档数据库、Web/客户端、安全与诊断组件的组合；自研平台当前采用 C++ 后端服务 + Vue Web 前端 + MySQL 的轻量分层架构。两者都具备“采集、处理、展示、存储、下发”的基本闭环，但工程深度和工业现场保障能力不同。"))
    parts.append(diagram("图 1 WinCC 典型 SCADA 架构示意", [
        "工程站 / TIA Portal 或 WinCC Engineering",
        "        | 画面、变量、报警、脚本、归档、权限配置",
        "        v",
        "WinCC Runtime / Server / Client / WebUX 或 Unified Web Client",
        "        |",
        "        +-- 工业通信：S7、OPC UA、OPC DA、MQTT、REST 等",
        "        +-- 归档与报警：过程值归档、报警归档、审计、报表",
        "        +-- 数据接口：SQL、开放 API、Web/IT 集成接口",
        "        v",
        "PLC / OPC UA Server / 现场设备 / 数据库 / MES / 第三方系统"
    ]))
    parts.append(diagram("图 2 自研监控平台当前架构示意", [
        "浏览器 / 运行包内置前端",
        "        | Vue 3 + Pinia + Chart.js + 画布组态组件",
        "        v",
        "C++ MonitoringRuntime.exe",
        "        | REST API / WebSocket / 静态资源服务",
        "        +-- OPC UA Server：对外发布变量",
        "        +-- OPC UA Client：远程读取/写入节点，支持 JSON 字典拆分",
        "        +-- UDP Receiver：接收 JSON 或 var=value 数据",
        "        +-- Data Manager：实时值、历史缓存、报警、操作记录",
        "        v",
        "MySQL software_plc_monitoring：变量定义、实时宽表、历史宽表、下发宽表、报警、日志、配置"
    ]))
    parts.append(table([
        ["层级", "WinCC", "自研平台", "差异说明"],
        ["工程层", "工程软件集中配置画面、变量、脚本、报警、归档、用户、安全、部署。", "前端项目管理 + 画布组态，项目数据主要在浏览器侧维护。", "WinCC 工程生命周期更完整；自研平台需要服务器侧项目库、版本管理、导入导出。"],
        ["运行层", "Runtime/Server/Client/Web，多种运行形态，可按授权扩展。", "单个 C++ 后端运行时，内置 HTTP、WebSocket、OPC UA、UDP。", "自研平台更轻；WinCC 更适合多站点、大客户端。"],
        ["通信层", "工业驱动生态丰富，支持 Siemens PLC 与多种开放协议。", "当前聚焦 OPC UA、UDP、JSON 字符串字典和模拟数据。", "自研平台协议面窄，但易定制。"],
        ["数据层", "内置归档、报警、审计、SQL/开放接口能力。", "MySQL 物理宽表：realtime_data、historical_data、write_records。", "自研表结构直观；WinCC 归档管理能力更成熟。"],
        ["访问层", "本地客户端、Web 客户端、WebUX/Unified Web 等。", "浏览器访问，运行包本机/LAN/Cloudflare 临时公网隧道。", "自研远程访问易用，但安全与稳定性需要加强。"]
    ], [1400, 2800, 2800, 2000], "表 3 架构分层对比"))

    parts.append(h1("三、开发平台与工程方式"))
    parts.append(table([
        ["项目", "WinCC", "自研平台", "评价"],
        ["工程环境", "WinCC Unified 常与 TIA Portal 统一工程环境配合；WinCC V8 提供面向 SCADA 的工程组态环境。", "前端 Vue/Vite，后端 C++/CMake，数据库 SQL 脚本，运行包构建脚本。", "自研平台开发自由度高，但工程工具链需要开发人员维护。"],
        ["画面开发", "图形编辑器、模板、面板、对象库、脚本、趋势、报警控件等成熟组件。", "Web 画布拖拽组件：趋势、数显、仪表、按钮、报警列表、指示灯、标签。", "自研组件已覆盖基础展示，缺少复杂对象库和工业面板体系。"],
        ["脚本/扩展", "WinCC Unified 偏 JavaScript/开放接口；V8 提供脚本、开放接口和扩展组件能力。", "通过源码扩展 Vue 组件、C++ API、数据库结构。", "自研扩展更底层、更灵活，但没有低代码工程化封装。"],
        ["调试诊断", "工业工程诊断、在线修改、日志、系统状态、通信诊断等。", "控制台日志、前端状态页、通信资源测试、OPC UA 浏览日志。", "自研平台可继续增加可视化诊断面板。"],
        ["团队协作", "商业工程项目可结合版本管理/工程规范/授权体系。", "当前依赖 Git 与本地文件，项目管理在前端 localStorage。", "需增加项目文件导入导出、服务端项目仓库、权限和审计。"]
    ], [1700, 3000, 3000, 1500], "表 4 开发平台与工程方式对比"))

    parts.append(table([
        ["自研平台代码模块", "主要职责", "对应 WinCC 能力"],
        ["backend/src/http_server.cpp", "REST API、WebSocket、静态资源、通信控制接口。", "运行时服务、Web 客户端接口。"],
        ["backend/src/opcua_manager.cpp", "OPC UA Server/Client、远程节点读写、JSON 字典解析、节点浏览诊断。", "工业通信、OPC UA 连接、变量发布。"],
        ["backend/src/udp_manager.cpp", "UDP 接收/发送、JSON 数值解析、变量自动创建。", "外部通信接口、第三方设备接入。"],
        ["backend/src/database.cpp", "MySQL 初始化、宽表迁移、实时/历史/下发数据写入。", "过程值归档、操作记录、配置存储。"],
        ["frontend/src/views/CanvasPage.vue", "组态画布、组件拖拽、配置、应用模式。", "HMI 画面编辑与运行。"],
        ["frontend/src/views/DataView.vue", "实时数据、报警、操作日志、数据库历史。", "实时值、报警视图、历史归档浏览。"],
        ["frontend/src/views/CommunicationView.vue", "通信资源配置、OPC UA/UDP 启停与测试。", "通信连接组态。"],
        ["frontend/src/store/index.js", "变量、实时数据、项目、组件配置和状态管理。", "变量表、画面对象状态。"]
    ], [2500, 4200, 2300], "表 5 自研平台源码结构与功能映射"))

    parts.append(h1("四、应用平台与部署运行"))
    parts.append(table([
        ["维度", "WinCC", "自研平台", "说明"],
        ["运行形态", "工业 PC Runtime、Server/Client、Web 客户端、Panel 等多种形态。", "MonitoringRuntime.exe 单机运行，浏览器访问前端。", "自研运行包类似轻量 runtime，不暴露前端源码。"],
        ["访问方式", "本地客户端、浏览器 Web 客户端、移动/远程 Web 方式，取决于版本和授权。", "本机 http://127.0.0.1:8081；局域网 http://本机IP:8081；可选 trycloudflare 公网隧道。", "公网临时隧道适合演示，不适合长期生产。"],
        ["安装交付", "安装包 + 授权 + 系统环境配置。", "release/MonitoringRuntime 目录含 exe、MySQL DLL、启动脚本、可选 cloudflared。", "自研平台交付简单，但缺少安装向导和服务化部署。"],
        ["服务化运行", "可按工业系统部署为服务器服务/运行站。", "当前由 bat/ps1 启动，控制台窗口运行。", "建议后续增加 Windows Service 模式。"],
        ["多用户访问", "支持多客户端/Web 客户端，按授权和架构扩展。", "HTTP/WebSocket 可被多浏览器访问；当前权限隔离较弱。", "需要会话、用户、角色和访问控制。"]
    ], [1600, 3000, 3000, 1600], "表 6 应用平台与运行部署对比"))

    parts.append(h1("五、数据库与历史数据模型"))
    parts.append(p("自研平台已按照你的要求调整为物理宽表：每张数据表第一列为 id，第二列为 time，后续按变量名动态增加列，适合横向查看同一时刻所有变量值。WinCC 更强调历史归档、报警归档、审计、报表和系统级数据管理，内部表结构通常不作为用户直接维护对象。"))
    parts.append(table([
        ["类别", "WinCC", "自研平台", "对比结论"],
        ["过程值归档", "内置过程值/历史数据归档，支持查询、趋势、报表等上层功能。", "historical_data 物理宽表，动态变量列，按 time 聚合。", "自研更直观；WinCC 更完整可靠。"],
        ["实时数据", "运行时维护变量当前值，可绑定画面对象、趋势、报警。", "realtime_data 物理宽表，保存最新或周期快照。", "自研结构适合外部 SQL 查看。"],
        ["下发记录", "操作/审计/变量写入可纳入日志和审计体系。", "write_records 物理宽表，记录 time 与每个下发变量值。", "自研已具备追溯基础。"],
        ["报警记录", "成熟报警归档、确认、分类、过滤、权限、审计。", "alarm_records 表记录变量、报警类型、限值、严重级别、确认状态。", "自研基础可用，需完善生命周期。"],
        ["操作日志", "支持系统/用户操作记录与审计，能力取决于选件。", "operation_log 保存写入、报警确认、操作详情。", "自研需增加用户身份和不可篡改审计。"],
        ["数据库类型", "WinCC Unified 公开资料强调 SQL 数据库归档；WinCC V8 提供集成归档/数据库能力。", "MySQL，库名 software_plc_monitoring。", "MySQL 便于二次开发和 Navicat 查看。"],
        ["数据维护", "归档管理、压缩、备份、恢复、长期运行机制较成熟。", "保留天数清理过程 archive_old_data；宽表字段动态维护。", "建议增加备份、分区、压缩与冷热数据策略。"]
    ], [1700, 3000, 3000, 1500], "表 7 数据库与历史数据模型对比"))
    parts.append(table([
        ["自研表名", "当前物理形态", "主要字段/动态列", "用途"],
        ["variable_definitions", "普通定义表", "id、name、description、data_type、source、opcua_node_id、udp_port、min/max、unit、color、enabled。", "变量/标签定义。"],
        ["realtime_data", "物理宽表", "id、time、变量名列...", "实时快照。"],
        ["historical_data", "物理宽表", "id、time、变量名列...", "历史趋势与数据库历史视图。"],
        ["write_records", "物理宽表", "id、time、变量名列...", "所有下发/写入数据追溯。"],
        ["alarm_records", "普通记录表", "var_id、var_name、alarm_type、alarm_value、limit_value、severity、acknowledged、occurred_at。", "报警记录与确认。"],
        ["operation_log", "普通记录表", "operation_type、var_name、old_value、new_value、operator、detail、operated_at。", "用户/系统操作日志。"],
        ["user_config", "配置表", "key、value、description、updated_at。", "系统配置，如 OPC UA 地址、UDP 端口、刷新间隔。"]
    ], [1800, 1700, 4300, 1500], "表 8 自研平台数据库表结构概览"))

    parts.append(h1("六、通信与数据采集功能"))
    parts.append(table([
        ["功能", "WinCC", "自研平台", "现状评价"],
        ["OPC UA 客户端", "支持连接 OPC UA 数据源，适用于跨厂商设备与系统集成。", "支持远程 OPC UA 节点读取、写入、浏览诊断；支持 nsu 与 ns 写法；支持字符串 JSON 字典拆分为多个变量。", "自研已具备核心能力。"],
        ["OPC UA 服务端", "可作为系统接口对外提供数据，能力与版本/配置相关。", "内置 OPC UA Server，对外发布平台变量，默认端口 4841。", "适合与 UaExpert/第三方系统联调。"],
        ["Siemens PLC 驱动", "对 S7 系列等 Siemens 控制器支持成熟。", "当前未实现专用 S7 驱动，主要通过 OPC UA 或 UDP 间接接入。", "若面向西门子现场，应增加 S7/Profinet 或通过 OPC UA 网关。"],
        ["UDP 数据", "WinCC 通常通过标准驱动/脚本/开放接口接入非标数据。", "内置 UDP Receiver，可解析 JSON 数值字段、varName=value、varId:value。", "自研对实验数据接入很灵活。"],
        ["REST/WebSocket", "WinCC V8/Unified 提供开放接口能力，具体接口随版本与选件变化。", "REST API 提供变量、实时、历史、报警、配置、写入；WebSocket 推送实时数据。", "自研 Web 交互链路清晰。"],
        ["MQTT/IT 集成", "WinCC 官方资料中包含 MQTT、REST/GraphQL 等面向 IT/云集成能力。", "当前未内置 MQTT；公网访问通过 cloudflared HTTP/WebSocket 隧道。", "建议增加 MQTT Client/Publisher。"]
    ], [1800, 3000, 3000, 1400], "表 9 通信与采集能力对比"))
    parts.append(table([
        ["自研 API 类别", "接口示例", "作用"],
        ["变量管理", "GET/POST/PUT/DELETE /api/variables", "变量定义增删改查。"],
        ["实时数据", "GET /api/realtime，WS /ws/realtime", "实时值查询与推送。"],
        ["历史数据", "GET /api/history/:id，GET /api/historydb/wide", "缓存历史与数据库历史查询。"],
        ["写入下发", "POST /api/write/:id", "下发变量值并写入 write_records。"],
        ["报警日志", "GET /api/alarms，POST /api/alarms/:id/ack，GET /api/logs", "报警查看/确认与操作日志。"],
        ["通信控制", "OPC UA 连接/节点/采集控制，UDP 启停接口", "从前端配置和启停通信资源。"],
        ["系统配置", "GET/POST /api/config/:key，GET /api/network/urls", "配置管理和访问地址提示。"]
    ], [2000, 3600, 3400], "表 10 自研平台 REST/WebSocket 接口概览"))

    parts.append(h1("七、画面、组件与可视化"))
    parts.append(table([
        ["功能类别", "WinCC", "自研平台", "差距/建议"],
        ["画面组态", "成熟图形编辑器、对象库、模板、画面导航、窗口、弹窗、脚本事件。", "浏览器画布拖拽，支持编辑模式和应用模式；应用模式锁定位置、大小、结构。", "建议增加图层、对齐吸附、组合、复制粘贴、撤销重做。"],
        ["趋势曲线", "历史/实时趋势、归档查询、缩放、游标、导出等成熟能力。", "Chart.js 趋势曲线，已修复实时更新、高 DPI 清晰度、缩放消失问题。", "建议增加游标、范围选择、导出、统计值。"],
        ["数值显示", "变量绑定、格式化、单位、颜色状态、脚本。", "digitalDisplay 支持绑定变量显示。", "建议增加格式模板和状态色。"],
        ["仪表盘", "内置仪表/第三方控件/自定义对象。", "gauge 组件用于量程展示。", "建议增加上下限、报警色带。"],
        ["控制按钮", "按钮对象可绑定脚本/变量写入/权限。", "button 组件已改为纯按钮样式，可触发下发。", "建议增加按钮权限、确认弹窗、互锁条件。"],
        ["报警列表", "强报警控件、过滤、确认、分类、优先级、归档。", "alarmList 组件和数据视图报警记录。", "建议完善报警生命周期。"],
        ["状态指示", "指示灯、状态图形、动态属性。", "indicator、label 等基础组件。", "建议增加动态图形库和面板模板。"]
    ], [1700, 3000, 3000, 1600], "表 11 可视化与画面组件对比"))

    parts.append(h1("八、报警、事件、历史趋势与报表"))
    parts.append(table([
        ["能力", "WinCC", "自研平台", "结论"],
        ["报警触发", "支持复杂报警类别、优先级、限值、文本、确认、归档。", "基于变量限值生成 alarm_records，记录严重级别和确认状态。", "基础链路可用。"],
        ["报警确认", "支持权限控制、确认状态、确认时间、操作追踪。", "POST /api/alarms/:id/ack，记录 acknowledged_at。", "需绑定用户身份与审计。"],
        ["事件/操作日志", "系统事件、用户操作、审计追踪能力成熟。", "operation_log 保存变量写入、报警确认等。", "需增加登录用户、IP、会话、不可篡改策略。"],
        ["趋势历史", "趋势控件与归档深度集成。", "历史宽表 + 趋势组件 + 数据库历史视图。", "适合横向查看变量，但高级分析待增强。"],
        ["报表", "可结合报表系统和归档输出生产报表。", "当前未实现正式报表模板/导出。", "建议增加 Excel/PDF/Word 报表生成。"],
        ["数据导出", "通常支持归档数据导出或通过开放接口取数。", "当前主要通过 API/数据库查看。", "建议前端增加 CSV/Excel 导出。"]
    ], [1800, 3100, 3000, 1400], "表 12 报警、事件、历史与报表对比"))

    parts.append(h1("九、项目管理与配置管理"))
    parts.append(table([
        ["功能", "WinCC", "自研平台", "评价"],
        ["项目创建/编辑", "工程项目完整管理，包含画面、变量、报警、脚本、用户等配置。", "项目列表、创建、编辑、详情、删除、复制；查询参数支持搜索/排序/分页。", "自研项目管理已具备基础流程。"],
        ["项目复制", "工程另存/复制/模板化能力成熟。", "已加入复制功能，可快速基于已有项目生成新项目。", "后续可加入模板库。"],
        ["删除流程", "工程级删除/备份/权限保护。", "软删除、二次确认、撤销提示。", "比简单 alert 更专业。"],
        ["配置独立性", "画面对象各自配置独立，可实例化。", "已修复同类型模块共用配置问题，拖出两个趋势曲线可分别配置变量等。", "符合组态软件基本预期。"],
        ["应用模式", "运行态与工程态分离。", "应用模式仅显示画布内容，按钮可用，位置/大小/结构不可改；退出后可继续编辑。", "基本运行态/编辑态边界已建立。"],
        ["配置存储", "工程文件/数据库/运行时配置统一管理。", "部分前端项目配置在 localStorage，后端 user_config 存系统配置。", "建议迁移到后端项目表并支持导入导出。"]
    ], [1900, 3000, 3000, 1400], "表 13 项目管理与配置管理对比"))

    parts.append(h1("十、安全、权限、审计与远程访问"))
    parts.append(table([
        ["维度", "WinCC", "自研平台", "风险与建议"],
        ["用户登录", "支持用户管理、角色权限、运行操作权限。", "当前前端有角色判断思路，但整体登录/会话体系不足。", "必须增加登录、密码策略、会话超时。"],
        ["操作权限", "按钮、画面、报警确认、工程操作可按权限控制。", "项目删除等 UI 权限已有雏形，后端仍需统一校验。", "后端必须作为最终权限边界。"],
        ["审计追踪", "工业平台通常支持审计/追踪，满足合规需求。", "operation_log 有记录基础，但缺用户身份、签名、不可篡改。", "增加审计表、用户、IP、结果码、旧新值、hash 链。"],
        ["通信安全", "支持工业安全机制、证书、加密和访问控制，能力取决于配置。", "本地 HTTP/WS 默认未启用 TLS；OPC UA 当前有用户名密码但日志提示未加密策略风险。", "公网访问必须加 HTTPS、鉴权、OPC UA 安全策略。"],
        ["公网访问", "企业级远程访问通常依赖 VPN、网关、DMZ、安全策略。", "提供 Cloudflare Quick Tunnel 临时公网访问。", "临时隧道适合演示；生产需固定域名、身份认证、访问控制。"],
        ["防误操作", "报警确认、变量写入、关键按钮可做权限/确认。", "删除已改内部确认弹窗；控制按钮可继续加二次确认。", "关键下发需要确认、互锁、权限、日志。"]
    ], [1700, 3100, 3000, 1600], "表 14 安全、权限、审计与远程访问对比"))

    parts.append(h1("十一、可扩展性、可靠性与运维"))
    parts.append(table([
        ["维度", "WinCC", "自研平台", "改进方向"],
        ["规模扩展", "支持服务器/客户端、大规模变量、归档与分布式部署，取决于版本授权。", "当前单机运行时，变量规模主要受进程、MySQL、前端渲染影响。", "增加分层采集服务、消息队列、分页/虚拟列表。"],
        ["冗余容错", "WinCC V8/Unified PC 官方资料包含冗余相关能力。", "当前无主备后端、无数据库主从切换。", "增加双机热备、MySQL 主从、心跳与故障切换。"],
        ["长期运行", "面向 7x24 工业现场，日志、归档、诊断体系成熟。", "可运行，但需要补长期稳定性测试、内存泄漏测试、断线重连测试。", "建立压力测试与守护进程。"],
        ["备份恢复", "工程备份、归档备份与系统恢复能力更成熟。", "当前未形成一键备份/恢复功能。", "增加项目、数据库、运行包版本备份。"],
        ["监控诊断", "通信诊断、系统状态、报警、运行日志。", "控制台日志、通信测试、网络地址页面。", "增加 Web 诊断面板和日志下载。"],
        ["升级发布", "商业版本升级与兼容策略。", "可重新构建运行包，前端嵌入 exe。", "增加版本号、迁移脚本、升级回滚。"]
    ], [1700, 3100, 3000, 1600], "表 15 可扩展性、可靠性与运维对比"))

    parts.append(h1("十二、功能大类与小功能清单"))
    parts.append(table([
        ["大类", "小功能", "WinCC", "自研平台当前状态"],
        ["变量/标签", "变量定义、单位、量程、颜色、启用状态", "成熟", "已实现 variable_definitions。"],
        ["变量/标签", "批量导入、模板、命名规则校验", "成熟/可配置", "待完善。"],
        ["通信", "OPC UA Client/Server", "成熟", "已实现。"],
        ["通信", "S7 驱动、第三方驱动库", "成熟", "未直接实现。"],
        ["通信", "UDP/JSON 非标数据接入", "可通过接口/脚本扩展", "已实现且灵活。"],
        ["实时监控", "实时表格、WebSocket 推送", "成熟", "已实现。"],
        ["历史", "历史归档、趋势、保留策略", "成熟", "已实现宽表与基础趋势，保留策略基础可用。"],
        ["下发控制", "变量写入、控制按钮、操作记录", "成熟", "已实现 write_records 与按钮下发基础。"],
        ["报警", "触发、显示、确认、归档", "成熟", "已实现基础触发/确认/记录。"],
        ["组态画面", "拖拽、移动、缩放、应用模式", "成熟", "已实现基础画布和应用模式。"],
        ["项目管理", "列表、创建、编辑、删除、复制", "成熟", "已实现基础流程和复制。"],
        ["安全", "登录、角色、权限、审计", "成熟", "部分 UI 雏形，后端需补齐。"],
        ["报表", "日报/月报、PDF/Excel", "成熟/可选件", "未实现。"],
        ["远程访问", "Web 客户端、企业远程访问", "成熟", "支持 LAN 和 Cloudflare 临时隧道。"],
        ["部署", "工程站/运行站/服务化", "成熟", "exe 运行包已实现，服务化待完善。"]
    ], [1400, 2600, 2000, 3000], "表 16 功能大类与小功能清单"))

    parts.append(h1("十三、成熟度评分矩阵"))
    parts.append(p("评分说明：5 = 工业级成熟；4 = 功能完整但依赖配置/授权；3 = 项目可用；2 = 部分实现；1 = 尚未实现。评分用于研发规划，不代表产品认证结论。"))
    parts.append(table([
        ["维度", "WinCC 评分", "自研平台评分", "主要依据"],
        ["总体架构", "5", "3", "自研已分层，但缺冗余、服务化、集中工程库。"],
        ["开发工程", "5", "3", "自研可拖拽组态，但缺对象库、脚本体系、版本协作。"],
        ["运行平台", "5", "3", "自研 exe 运行包可用，缺服务化和高可用。"],
        ["数据库/历史", "5", "3", "宽表直观，归档压缩、备份恢复、长期性能需增强。"],
        ["通信协议", "5", "3", "OPC UA/UDP 已有，驱动生态仍窄。"],
        ["可视化组件", "5", "3", "基础组件可用，复杂工业图库与交互不足。"],
        ["报警事件", "5", "2", "基础报警有了，生命周期和审计不足。"],
        ["项目管理", "5", "3", "已实现 CRUD/复制/软删除，服务端项目库待补。"],
        ["安全权限", "5", "1", "这是自研平台最需要优先补齐的短板。"],
        ["远程部署", "4", "3", "自研公网演示方便，但生产安全与稳定性需重做。"],
        ["报表分析", "4", "1", "当前未实现正式报表。"]
    ], [2200, 1500, 1500, 4800], "表 17 功能成熟度评分矩阵"))

    parts.append(h1("十四、差距总结与改进路线"))
    parts.append(table([
        ["优先级", "改进项", "解决的问题", "建议实现方式"],
        ["P0", "用户登录、角色权限、后端鉴权", "防止未授权访问、误操作和公网暴露风险。", "新增 user、role、permission 表；JWT/Session；所有写入/删除/确认接口后端校验。"],
        ["P0", "关键控制下发二次确认与审计", "防止误操作，满足追溯。", "按钮增加确认配置；operation_log 增加 user、ip、result、request_id、old/new value。"],
        ["P0", "HTTPS/WSS 与 OPC UA 安全策略", "解决公网/局域网明文访问风险。", "反向代理 TLS 或内置 TLS；OPC UA 启用证书、安全策略和用户权限。"],
        ["P1", "服务端项目库", "解决 localStorage 项目难共享、难备份的问题。", "新增 projects、project_versions、project_assets 表；支持导入/导出 JSON。"],
        ["P1", "报警生命周期完善", "接近工业报警管理。", "增加报警规则、确认人、恢复时间、抑制、过滤、统计。"],
        ["P1", "历史数据分区与备份", "解决长期运行表膨胀。", "MySQL 按日期分区；自动备份；冷热数据归档。"],
        ["P1", "组件库增强", "提升画面工程效率。", "增加管道、阀门、电机、泵、罐、流程图图元和面板模板。"],
        ["P2", "报表导出", "满足管理和实验记录。", "CSV/Excel/PDF/Word 报表模板。"],
        ["P2", "MQTT/REST 开放接口增强", "增强 IT/云集成。", "MQTT 发布实时/报警；REST API 文档化。"],
        ["P2", "Windows Service 与守护", "提升生产运行稳定性。", "注册服务、自动重启、日志滚动、健康检查。"],
        ["P3", "冗余与多实例部署", "面向生产级高可用。", "后端主备、MySQL 主从、心跳切换、Web 负载均衡。"]
    ], [1000, 2300, 2600, 3100], "表 18 自研平台改进路线图"))

    parts.append(h1("十五、适用场景建议"))
    parts.append(table([
        ["场景", "建议平台", "原因"],
        ["实验室教学、原型验证、课题演示", "自研平台优先", "部署轻、源码可改、MySQL 数据直观、OPC UA/UDP/JSON 接入方便。"],
        ["小型设备监控、非关键生产展示屏", "自研平台可用，需加权限和备份", "已有采集、显示、趋势、报警、日志和运行包。"],
        ["连续生产线 SCADA、多人运维、长期 7x24", "WinCC 优先", "需要成熟报警、归档、安全、冗余、驱动、工程规范和商业支持。"],
        ["企业级 Web 远程监控", "WinCC 或自研平台加强后再用", "自研需补 HTTPS、登录、权限、审计、固定域名、访问控制。"],
        ["需要深度定制数据库结构/算法/专用界面", "自研平台更合适", "源码和数据库完全可控，开发灵活。"]
    ], [2500, 2500, 4000], "表 19 应用场景选型建议"))

    parts.append(h1("十六、参考资料"))
    parts.append(p("以下资料用于校验 WinCC 侧公开能力。自研平台侧依据当前仓库源码、数据库初始化脚本和 release/MonitoringRuntime 运行包核对。"))
    parts.append(table([
        ["序号", "资料", "链接/位置", "用于报告中的信息"],
        ["1", "Siemens SIMATIC WinCC Unified 官方产品页", "https://www.siemens.com/global/en/products/automation/simatic-hmi/wincc-unified.html", "Web 客户端、平台统一性、Unified 运行体系、OPC UA/MQTT/GraphQL/SQL/冗余/审计等公开功能归纳。"],
        ["2", "Siemens SCADA system SIMATIC WinCC Unified 与 WinCC V8 官方页", "https://www.siemens.com/global/en/products/automation/industry-software/automation-software/scada.html", "SCADA 产品定位、WinCC Unified 与 WinCC V8 功能方向、WebUX、REST/MQTT、IEC 62443、安全等公开功能归纳。"],
        ["3", "本项目后端 API 声明", "backend/include/http_server.h", "REST API、WebSocket、变量、历史、报警、写入、配置接口。"],
        ["4", "本项目数据库初始化脚本", "backend/sql/init.sql", "MySQL 表结构、宽表、报警、日志、配置、保留策略。"],
        ["5", "本项目运行包目录", "release/MonitoringRuntime", "MonitoringRuntime.exe、libmysql.dll、启动脚本、Cloudflare 隧道脚本。"],
        ["6", "本项目前端依赖", "frontend/package.json", "Vue 3、Pinia、Chart.js、Axios、Vite 等。"]
    ], [800, 2600, 3600, 3000], "表 20 参考资料与源码依据"))

    parts.append(h1("十七、最终总结"))
    parts.append(p("WinCC 的核心价值是“工业工程体系”：它不仅能显示数据，更覆盖从工程组态、通信、归档、报警、安全、客户端访问到长期运维的完整链路。自研平台的核心价值是“可控和贴合需求”：当前已经具备监控平台的主链路，并且前后端、数据库、运行包都能按你的需求快速修改。"))
    parts.append(p("如果目标是做一个可演示、可部署、可持续扩展的自研监控平台，当前架构方向是成立的；下一阶段最优先补齐安全权限、审计、服务端项目库、报警生命周期、历史数据维护和服务化运行。完成这些以后，自研平台会更接近轻量 SCADA，而不是普通数据看板。"))

    parts.append('''
<w:sectPr>
  <w:pgSz w:w="11906" w:h="16838"/>
  <w:pgMar w:top="1440" w:right="1080" w:bottom="1080" w:left="1080" w:header="720" w:footer="720" w:gutter="0"/>
  <w:cols w:space="720"/>
  <w:docGrid w:linePitch="312"/>
</w:sectPr>''')
    parts.append("</w:body></w:document>")
    return "".join(parts)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    files = {
        "[Content_Types].xml": content_types_xml(),
        "_rels/.rels": rels_xml(),
        "word/_rels/document.xml.rels": doc_rels_xml(),
        "word/document.xml": make_document_xml(),
        "word/styles.xml": styles_xml(),
        "word/settings.xml": settings_xml(),
        "docProps/core.xml": core_xml(),
        "docProps/app.xml": app_xml(),
    }
    with zipfile.ZipFile(OUT_FILE, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(name, data.encode("utf-8"))
    print(OUT_FILE)


if __name__ == "__main__":
    main()
