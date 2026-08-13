<template>
  <div class="process-device" :class="[deviceClass, statusClass]" :style="deviceStyle">
    <svg v-if="widget.type === 'processPump'" :key="`${refreshKey}-pump`" viewBox="0 0 120 96" aria-hidden="true">
      <defs>
        <radialGradient id="pumpBody" cx="45%" cy="42%" r="58%">
          <stop offset="0" stop-color="#ffffff" />
          <stop offset="0.58" stop-color="#dce4ea" />
          <stop offset="1" stop-color="#8c9aa4" />
        </radialGradient>
      </defs>
      <circle class="body" cx="60" cy="50" r="30" fill="url(#pumpBody)" />
      <circle class="core" cx="60" cy="50" r="13" />
      <path class="impeller" d="M60 37l8 13-8 13-8-13zM47 50l13-8 13 8-13 8z" />
      <path class="base" d="M42 82h36l8 9H34z" />
      <circle class="state-dot" cx="93" cy="76" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processPool'" :key="`${refreshKey}-pool`" viewBox="0 0 380 156" preserveAspectRatio="none" aria-hidden="true">
      <defs>
        <linearGradient id="poolWater" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stop-color="#8ed7f0" />
          <stop offset="1" stop-color="#789096" />
        </linearGradient>
      </defs>
      <path class="wall" d="M14 18v118h134l26-15h46l26 15h120V18" />
      <path class="water" :d="poolWaterPath" fill="url(#poolWater)" />
      <path class="wave" d="M42 62c18-12 31 12 49 0s31 12 49 0 31 12 49 0 31 12 49 0 31 12 49 0" />
      <path class="wave soft" d="M85 92c14-9 24 9 38 0s24 9 38 0 24 9 38 0 24 9 38 0" />
    </svg>

    <svg v-else-if="widget.type === 'processDam'" :key="`${refreshKey}-dam`" viewBox="0 0 540 140" preserveAspectRatio="none" aria-hidden="true">
      <defs>
        <pattern id="damHatch" width="18" height="18" patternUnits="userSpaceOnUse">
          <path d="M-4 18L18-4M0 22L22 0M14 22L22 14" stroke="#111827" stroke-width="1.2" />
          <path d="M-4 0L18 22M0-4L22 18M14-4L22 4" stroke="#111827" stroke-width="1.2" />
        </pattern>
      </defs>
      <rect class="dam-body" x="3" y="18" width="520" height="92" fill="url(#damHatch)" />
      <path class="dam-side" d="M523 18h12v92h-12" />
      <path class="dam-top" d="M3 18h520" />
      <path class="dam-bottom" d="M3 110h520" />
    </svg>

    <svg v-else-if="widget.type === 'processTank'" :key="`${refreshKey}-tank`" viewBox="0 0 92 136" aria-hidden="true">
      <defs>
        <linearGradient id="tankMetal" x1="0" y1="0" x2="1" y2="0">
          <stop offset="0" stop-color="#8a9398" />
          <stop offset="0.18" stop-color="#f7f9fa" />
          <stop offset="0.52" stop-color="#c8d0d4" />
          <stop offset="0.82" stop-color="#ffffff" />
          <stop offset="1" stop-color="#778188" />
        </linearGradient>
      </defs>
      <ellipse class="tank-top" cx="46" cy="18" rx="24" ry="12" />
      <path class="tank-body" d="M22 18v86c0 7 11 13 24 13s24-6 24-13V18" fill="url(#tankMetal)" />
      <path class="tank-level" :d="tankLevelPath" />
      <ellipse class="tank-bottom" cx="46" cy="104" rx="24" ry="12" />
      <path class="tank-pipe" d="M46 0v6M46 116v20M30 130h32" />
    </svg>

    <svg v-else-if="widget.type === 'processMixer'" :key="`${refreshKey}-mixer`" viewBox="0 0 280 210" aria-hidden="true">
      <defs>
        <linearGradient id="mixerBowl" x1="0" y1="0" x2="1" y2="1">
          <stop offset="0" stop-color="#b8fbff" />
          <stop offset="0.35" stop-color="#38d5ee" />
          <stop offset="1" stop-color="#1586b5" />
        </linearGradient>
      </defs>
      <ellipse class="mixer-rim" cx="140" cy="74" rx="104" ry="44" />
      <path class="mixer-wall" d="M36 74c0 38 28 80 104 80s104-42 104-80v56c0 42-47 70-104 70S36 172 36 130z" fill="url(#mixerBowl)" />
      <ellipse class="mixer-water" cx="140" cy="76" rx="86" ry="32" />
      <path class="mixer-bridge" d="M48 46l184 48" />
      <circle class="mixer-hub" cx="140" cy="74" r="10" />
      <path class="mixer-leg" d="M140 151v36" />
    </svg>

    <svg v-else-if="widget.type === 'processDosingMachine'" :key="`${refreshKey}-dosing`" viewBox="0 0 96 104" aria-hidden="true">
      <rect class="dosing-frame" x="18" y="24" width="60" height="72" rx="2" />
      <rect class="dosing-motor" x="39" y="10" width="18" height="28" rx="3" />
      <path class="dosing-shaft" d="M48 38v39" />
      <path class="dosing-impeller" d="M38 77h20M38 77l-5 5M58 77l5 5" />
      <circle class="dosing-hub" cx="48" cy="77" r="3" />
      <circle class="state-dot" cx="70" cy="87" r="4" />
    </svg>

    <svg
      v-else-if="widget.type === 'processJunction'"
      :key="`${refreshKey}-junction`"
      class="junction-svg"
      :class="{ 'junction-svg-hidden': hideJunctionNode }"
      viewBox="0 0 12 12"
      aria-hidden="true"
    >
      <circle class="junction-node" cx="6" cy="6" r="5" />
    </svg>

    <svg v-else-if="widget.type === 'processValve'" :key="`${refreshKey}-valve`" viewBox="0 0 100 56" aria-hidden="true">
      <path class="valve-body" d="M20 12l30 16-30 16zM80 12L50 28l30 16z" />
      <rect class="valve-stem" x="47" y="5" width="6" height="18" rx="2" />
      <rect class="valve-handle" x="34" y="0" width="32" height="7" rx="2" />
      <circle class="state-dot" cx="86" cy="42" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processValveVertical'" :key="`${refreshKey}-valve-v`" viewBox="0 0 58 88" aria-hidden="true">
      <path class="valve-body" d="M13 16l16 28 16-28zM13 72l16-28 16 28z" />
      <rect class="valve-stem" x="20" y="40" width="18" height="7" rx="2" />
      <rect class="valve-handle" x="14" y="8" width="30" height="7" rx="2" />
      <circle class="state-dot" cx="46" cy="70" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processPipeEnd'" :key="`${refreshKey}-pipe-end`" viewBox="0 0 82 34" aria-hidden="true">
      <path class="pipe-end-line" d="M4 17h65" />
      <path class="pipe-end-head" d="M64 9h14l-5 8 5 8H64z" />
    </svg>

    <svg v-else-if="widget.type === 'processCalcinerFurnace'" :key="`${refreshKey}-calciner`" viewBox="0 0 132 238" aria-hidden="true">
      <defs>
        <linearGradient id="calcinerShell" x1="0" y1="0" x2="1" y2="0">
          <stop offset="0" stop-color="#8a9398" />
          <stop offset="0.22" stop-color="#f7f9fa" />
          <stop offset="0.56" stop-color="#cbd5dc" />
          <stop offset="0.82" stop-color="#ffffff" />
          <stop offset="1" stop-color="#768089" />
        </linearGradient>
      </defs>
      <path class="calciner-shell" d="M42 18h48c8 0 14 8 14 18v126c0 24-16 44-38 44s-38-20-38-44V36c0-10 6-18 14-18z" fill="url(#calcinerShell)" />
      <ellipse class="calciner-top" cx="66" cy="36" rx="38" ry="16" />
      <path class="calciner-band" d="M30 80h72M30 122h72M32 164h68" />
      <path class="calciner-bed" d="M36 146c16-8 44 8 60 0v28c-14 14-48 14-60 0z" />
      <path class="calciner-cone" d="M36 178h60l-20 40H56z" />
      <circle class="state-dot" cx="106" cy="198" r="5" />
    </svg>

    <svg v-else-if="widget.type === 'processCycloneSeparator'" :key="`${refreshKey}-cyclone`" viewBox="0 0 92 142" aria-hidden="true">
      <defs>
        <linearGradient id="cycloneMetal" x1="0" y1="0" x2="1" y2="0">
          <stop offset="0" stop-color="#8b969d" />
          <stop offset="0.28" stop-color="#ffffff" />
          <stop offset="0.62" stop-color="#cdd6dc" />
          <stop offset="1" stop-color="#6f7b84" />
        </linearGradient>
      </defs>
      <path class="cyclone-body" d="M24 18h44v54c0 7-10 13-22 13S24 79 24 72z" fill="url(#cycloneMetal)" />
      <ellipse class="cyclone-top" cx="46" cy="18" rx="22" ry="10" />
      <path class="cyclone-cone" d="M25 70h42l-14 54H39z" fill="url(#cycloneMetal)" />
      <path class="cyclone-vortex" d="M60 34c-22-10-35 11-16 20s24-9 8-15" />
      <circle class="state-dot" cx="72" cy="116" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processPreheaterTower'" :key="`${refreshKey}-preheater`" viewBox="0 0 84 184" aria-hidden="true">
      <rect class="preheater-shell" x="20" y="16" width="44" height="152" rx="8" />
      <path class="preheater-coil" d="M27 36h30M57 36v18H27v18h30v18H27v18h30v18H27v18h30" />
      <path class="preheater-band" d="M21 58h42M21 100h42M21 142h42" />
      <circle class="state-dot" cx="70" cy="160" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processBagFilter'" :key="`${refreshKey}-bag-filter`" viewBox="0 0 246 132" aria-hidden="true">
      <rect class="filter-house" x="28" y="16" width="190" height="68" rx="2" />
      <path class="filter-roof" d="M22 16h202M28 32h190M28 66h190" />
      <path class="filter-bags" d="M48 34v34M70 34v34M92 34v34M114 34v34M136 34v34M158 34v34M180 34v34M202 34v34" />
      <g class="filter-indicators">
        <circle cx="52" cy="45" r="5" /><circle cx="72" cy="45" r="5" /><circle cx="92" cy="45" r="5" /><circle cx="112" cy="45" r="5" />
        <circle cx="132" cy="45" r="5" /><circle cx="152" cy="45" r="5" /><circle cx="172" cy="45" r="5" /><circle cx="192" cy="45" r="5" />
        <circle cx="52" cy="61" r="5" /><circle cx="72" cy="61" r="5" /><circle cx="92" cy="61" r="5" /><circle cx="112" cy="61" r="5" />
        <circle cx="132" cy="61" r="5" /><circle cx="152" cy="61" r="5" /><circle cx="172" cy="61" r="5" /><circle cx="192" cy="61" r="5" />
      </g>
      <path class="filter-hopper" d="M36 84h42l-14 34H50zM78 84h42l-14 34H92zM120 84h42l-14 34h-14zM162 84h42l-14 34h-14z" />
      <path class="filter-outlet" d="M64 118v14M106 118v14M148 118v14M190 118v14" />
      <circle class="state-dot" cx="210" cy="106" r="5" />
    </svg>

    <svg v-else-if="widget.type === 'processFanBlower'" :key="`${refreshKey}-fan`" viewBox="0 0 122 104" aria-hidden="true">
      <path class="fan-scroll" d="M62 15c27 0 44 16 44 36 0 23-18 38-43 38H28V54c0-23 14-39 34-39z" />
      <circle class="fan-wheel" cx="59" cy="53" r="24" />
      <path class="fan-blades" d="M59 29l9 19-9 6-9-6zM83 53l-19 9-6-9 6-9zM59 77l-9-19 9-6 9 6zM35 53l19-9 6 9-6 9z" />
      <path class="fan-base" d="M40 88h44l9 10H31z" />
      <circle class="state-dot" cx="96" cy="82" r="5" />
    </svg>

    <svg v-else-if="widget.type === 'processWinccFan'" :key="`${refreshKey}-wincc-fan`" viewBox="0 0 96 96" aria-hidden="true">
      <defs>
        <radialGradient id="winccFanBody" cx="42%" cy="36%" r="66%">
          <stop offset="0" stop-color="#a9ff47" />
          <stop offset="0.58" stop-color="#58e319" />
          <stop offset="1" stop-color="#28b40d" />
        </radialGradient>
      </defs>
      <path class="wincc-fan-port" d="M48 0v20" />
      <path class="wincc-fan-base" d="M34 72h28l7 12H27z" />
      <circle class="wincc-fan-body" cx="48" cy="50" r="27" fill="url(#winccFanBody)" />
      <circle class="wincc-fan-ring" cx="48" cy="50" r="19" />
      <path class="wincc-fan-blades" d="M48 31c8 6 12 12 11 19-5-4-10-6-16-5 0-6 2-10 5-14zM67 50c-6 8-12 12-19 11 4-5 6-10 5-16 6 0 10 2 14 5zM48 69c-8-6-12-12-11-19 5 4 10 6 16 5 0 6-2 10-5 14zM29 50c6-8 12-12 19-11-4 5-6 10-5 16-6 0-10-2-14-5z" />
      <circle class="wincc-fan-core" cx="48" cy="50" r="9" />
      <path class="wincc-fan-motor-link" d="M67 34h8" />
      <rect class="wincc-fan-motor" x="75" y="27" width="15" height="15" rx="2" />
      <text class="wincc-fan-motor-text" x="82.5" y="38">M</text>
      <circle class="state-dot" cx="76" cy="68" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processBurner'" :key="`${refreshKey}-burner`" viewBox="0 0 126 70" aria-hidden="true">
      <path class="burner-gas-line" d="M0 36h34" />
      <rect class="burner-body" x="30" y="22" width="48" height="28" rx="4" />
      <path class="burner-nozzle" d="M78 28h20l16 8-16 8H78z" />
      <path class="burner-flame outer" d="M112 18c18 14 16 31 0 44-6-11-18-15-8-28 3-4 6-8 8-16z" />
      <path class="burner-flame inner" d="M112 30c8 8 7 16 0 24-4-7-9-9-5-16 2-2 3-5 5-8z" />
      <circle class="state-dot" cx="44" cy="58" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processAluminaSilo'" :key="`${refreshKey}-alumina-silo`" viewBox="0 0 106 156" aria-hidden="true">
      <defs>
        <linearGradient id="siloShell" x1="0" y1="0" x2="1" y2="0">
          <stop offset="0" stop-color="#87919a" />
          <stop offset="0.25" stop-color="#f8fafc" />
          <stop offset="0.58" stop-color="#d6dde3" />
          <stop offset="1" stop-color="#6f7a83" />
        </linearGradient>
      </defs>
      <path class="silo-shell" d="M24 12h58v82c0 9-13 16-29 16S24 103 24 94z" fill="url(#siloShell)" />
      <rect class="silo-fill" x="28" :y="96 - fillPercent * 0.64" width="50" :height="Math.max(8, fillPercent * 0.64)" />
      <ellipse class="silo-top" cx="53" cy="12" rx="29" ry="12" />
      <path class="silo-cone" d="M26 94h54l-18 44H44z" fill="url(#siloShell)" />
      <circle class="state-dot" cx="86" cy="132" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processScrewConveyor'" :key="`${refreshKey}-screw`" viewBox="0 0 260 62" aria-hidden="true">
      <rect class="screw-trough" x="18" y="18" width="224" height="26" rx="4" />
      <path class="screw-flight" d="M28 31c10-22 22 22 32 0s22-22 32 0 22 22 32 0 22-22 32 0 22 22 32 0 22-22 32 0" />
      <circle class="screw-drive" cx="224" cy="31" r="13" />
      <circle class="state-dot" cx="244" cy="48" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processBucketElevator'" :key="`${refreshKey}-elevator`" viewBox="0 0 82 214" aria-hidden="true">
      <rect class="elevator-case" x="22" y="12" width="38" height="190" rx="4" />
      <circle class="elevator-pulley" cx="41" cy="32" r="14" />
      <circle class="elevator-pulley" cx="41" cy="182" r="14" />
      <path class="elevator-belt" d="M34 32v150M48 32v150" />
      <path class="elevator-buckets" d="M34 58h14l-4 12H30zM34 92h14l-4 12H30zM34 126h14l-4 12H30zM34 160h14l-4 12H30z" />
      <circle class="state-dot" cx="68" cy="194" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processGasValveSkid'" :key="`${refreshKey}-gas-skid`" viewBox="0 0 184 94" aria-hidden="true">
      <rect class="gas-skid-frame" x="8" y="12" width="168" height="70" rx="4" />
      <path class="gas-line" d="M0 48h184" />
      <circle class="gas-valve" cx="48" cy="48" r="13" />
      <circle class="gas-valve" cx="102" cy="48" r="13" />
      <path class="gas-valve-handle" d="M36 36l24 24M90 60l24-24" />
      <circle class="gas-gauge" cx="144" cy="34" r="14" />
      <path class="gas-gauge-needle" d="M144 34l8-7" />
      <circle class="state-dot" cx="162" cy="70" r="5" />
    </svg>

    <svg v-else-if="widget.type === 'processHeatExchanger'" :key="`${refreshKey}-heat-exchanger`" viewBox="0 0 86 178" aria-hidden="true">
      <rect class="heat-shell" x="20" y="14" width="46" height="150" rx="4" />
      <path class="heat-coil" d="M29 30h28v14H29v14h28v14H29v14h28v14H29v14h28v14H29v14h28" />
      <path class="heat-tube-bank" d="M31 22v134M43 22v134M55 22v134" />
      <path class="heat-band" d="M21 48h44M21 88h44M21 128h44" />
      <circle class="state-dot" cx="70" cy="154" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processKilnTube'" :key="`${refreshKey}-kiln-tube`" viewBox="0 0 320 86" aria-hidden="true">
      <path class="kiln-shell" d="M24 22h246c16 0 30 9 30 21s-14 21-30 21H24z" />
      <ellipse class="kiln-end" cx="28" cy="43" rx="18" ry="21" />
      <ellipse class="kiln-end right" cx="270" cy="43" rx="30" ry="21" />
      <path class="kiln-ribs" d="M62 23v40M92 23v40M122 23v40M152 23v40M182 23v40M212 23v40M242 23v40" />
      <path class="kiln-rollers" d="M72 64l-10 14h28l-10-14M220 64l-10 14h28l-10-14" />
      <circle class="kiln-drive" cx="286" cy="43" r="10" />
      <circle class="state-dot" cx="300" cy="70" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processStack'" :key="`${refreshKey}-stack`" viewBox="0 0 64 240" aria-hidden="true">
      <defs>
        <linearGradient id="stackMetal" x1="0" y1="0" x2="1" y2="0">
          <stop offset="0" stop-color="#7f8991" />
          <stop offset="0.35" stop-color="#f8fafc" />
          <stop offset="0.72" stop-color="#cbd5dc" />
          <stop offset="1" stop-color="#6f7a83" />
        </linearGradient>
      </defs>
      <path class="stack-body" d="M22 18h20l8 196H14z" fill="url(#stackMetal)" />
      <ellipse class="stack-top" cx="32" cy="18" rx="12" ry="6" />
      <path class="stack-bands" d="M19 60h26M18 112h28M16 164h32" />
      <path class="stack-base" d="M8 214h48l6 18H2z" />
      <path class="stack-smoke" d="M28 8c-10-12 14-10 4-24M42 10c14-10-8-15 3-26" />
    </svg>

    <svg v-else-if="widget.type === 'processRotaryAirlock'" :key="`${refreshKey}-airlock`" viewBox="0 0 74 74" aria-hidden="true">
      <circle class="airlock-body" cx="37" cy="37" r="24" />
      <path class="airlock-star" d="M37 17l6 15 16-5-10 13 14 9-17 1 1 17-10-14-10 14 1-17-17-1 14-9-10-13 16 5z" />
      <circle class="state-dot" cx="58" cy="58" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processDamper'" :key="`${refreshKey}-damper`" viewBox="0 0 96 58" aria-hidden="true">
      <rect class="damper-frame" x="18" y="12" width="60" height="34" rx="3" />
      <path class="damper-blade" d="M26 42l44-26" />
      <path class="damper-shaft" d="M48 8v42" />
      <circle class="damper-actuator" cx="76" cy="12" r="8" />
      <circle class="state-dot" cx="82" cy="46" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processSprayLance'" :key="`${refreshKey}-spray-lance`" viewBox="0 0 138 52" aria-hidden="true">
      <rect class="spray-body" x="26" y="18" width="52" height="16" rx="3" />
      <path class="spray-nozzle" d="M78 20h22l18 6-18 6H78z" />
      <path class="spray-fan" d="M116 18l20-12M118 26h20M116 34l20 12" />
      <circle class="state-dot" cx="36" cy="42" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processMaterialChute'" :key="`${refreshKey}-material-chute`" viewBox="0 0 120 86" aria-hidden="true">
      <path class="chute-body" d="M8 18h34l70 38-12 18L28 36H8z" />
      <path class="chute-liner" d="M30 30l70 38M44 36l52 28" />
    </svg>

    <svg v-else-if="widget.type === 'processDoubleFlapValve'" :key="`${refreshKey}-double-flap`" viewBox="0 0 70 100" aria-hidden="true">
      <rect class="flap-shell" x="18" y="12" width="34" height="76" rx="3" />
      <path class="flap-plate" d="M21 34l28-14M21 66l28-14" />
      <path class="flap-divider" d="M18 50h34" />
      <circle class="flap-actuator" cx="56" cy="30" r="6" />
      <circle class="flap-actuator" cx="56" cy="62" r="6" />
      <circle class="state-dot" cx="58" cy="84" r="4" />
    </svg>

    <svg v-else-if="widget.type === 'processStatusMatrix'" :key="`${refreshKey}-status-matrix`" viewBox="0 0 178 112" aria-hidden="true">
      <rect class="status-panel" x="4" y="4" width="170" height="104" rx="4" />
      <text class="status-title" x="16" y="22">联锁 / 运行状态</text>
      <g class="status-row">
        <text x="18" y="44">引风</text><circle class="lamp-ok" cx="74" cy="39" r="6" /><circle class="lamp-ok" cx="96" cy="39" r="6" /><circle class="lamp-alarm" cx="132" cy="39" r="6" /><circle class="lamp-idle" cx="154" cy="39" r="6" />
      </g>
      <g class="status-row">
        <text x="18" y="68">燃气</text><circle class="lamp-ok" cx="74" cy="63" r="6" /><circle class="lamp-idle" cx="96" cy="63" r="6" /><circle class="lamp-ok" cx="132" cy="63" r="6" /><circle class="lamp-alarm" cx="154" cy="63" r="6" />
      </g>
      <g class="status-row">
        <text x="18" y="92">给料</text><circle class="lamp-ok" cx="74" cy="87" r="6" /><circle class="lamp-ok" cx="96" cy="87" r="6" /><circle class="lamp-ok" cx="132" cy="87" r="6" /><circle class="lamp-idle" cx="154" cy="87" r="6" />
      </g>
    </svg>

    <svg
      v-if="portLineSegments.length"
      class="process-port-lines"
      viewBox="0 0 100 100"
      preserveAspectRatio="none"
      aria-hidden="true"
    >
      <line
        v-for="line in portLineSegments"
        :key="line.key"
        class="process-port-line"
        :x1="line.x1"
        :y1="line.y1"
        :x2="line.x2"
        :y2="line.y2"
      />
    </svg>

    <div
      v-if="deviceLabel && !hideName"
      class="device-label"
      :class="{ draggable: !readonly }"
      :style="labelStyle"
      title="拖动调整名称位置"
      @mousedown.stop.prevent="startLabelDrag"
    >
      {{ deviceLabel }}
    </div>
  </div>
</template>

<script setup>
import { computed, inject, ref } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({
  widget: { type: Object, required: true },
  readonly: { type: Boolean, default: false }
})
const store = useMonitorStore()
const canvasZoom = inject('canvasZoom', ref(1))
const zoomRefreshTick = inject('zoomRefreshTick', ref(0))

const DEFAULT_PROCESS_PORTS = {
  processPump: {
    left: { x: 5 / 120, y: 51 / 96 },
    right: { x: 115 / 120, y: 51 / 96 }
  },
  processDosingMachine: {
    left: { x: 0, y: 54 / 104 },
    right: { x: 1, y: 54 / 104 }
  },
  processValve: {
    left: { x: 0, y: 0.5 },
    right: { x: 1, y: 0.5 }
  },
  processValveVertical: {
    top: { x: 0.5, y: 0 },
    bottom: { x: 0.5, y: 1 }
  },
  processCalcinerFurnace: {
    top: { x: 0.5, y: 0 },
    right: { x: 1, y: 120 / 238 },
    bottom: { x: 0.5, y: 1 },
    left: { x: 0, y: 62 / 238 }
  },
  processCycloneSeparator: {
    top: { x: 0.5, y: 0 },
    bottom: { x: 0.5, y: 1 },
    left: { x: 0, y: 38 / 142 }
  },
  processPreheaterTower: {
    top: { x: 0.5, y: 0 },
    right: { x: 1, y: 132 / 184 },
    bottom: { x: 0.5, y: 1 },
    left: { x: 0, y: 52 / 184 }
  },
  processBagFilter: {
    left: { x: 0, y: 36 / 132 },
    right: { x: 1, y: 36 / 132 },
    bottom: { x: 0.5, y: 1 }
  },
  processFanBlower: {
    left: { x: 0, y: 54 / 104 },
    right: { x: 1, y: 54 / 104 },
    top: { x: 62 / 122, y: 0 }
  },
  processWinccFan: {
    top: { x: 0.5, y: 0 }
  },
  processAluminaSilo: {
    top: { x: 0.5, y: 0 },
    bottom: { x: 0.5, y: 1 }
  },
  processScrewConveyor: {
    left: { x: 0, y: 0.5 },
    right: { x: 1, y: 0.5 }
  },
  processBucketElevator: {
    right: { x: 1, y: 32 / 214 },
    left: { x: 0, y: 166 / 214 }
  },
  processHeatExchanger: {
    top: { x: 0.5, y: 0 },
    bottom: { x: 0.5, y: 1 }
  },
  processKilnTube: {
    left: { x: 0, y: 0.5 },
    right: { x: 1, y: 0.5 }
  },
  processRotaryAirlock: {
    top: { x: 0.5, y: 0 }
  },
  processDamper: {
    left: { x: 0, y: 0.5 },
    right: { x: 1, y: 0.5 }
  },
  processSprayLance: {
    left: { x: 0, y: 0.5 },
    right: { x: 1, y: 0.5 }
  },
  processMaterialChute: {
    left: { x: 0, y: 27 / 86 },
    right: { x: 1, y: 65 / 86 }
  },
  processDoubleFlapValve: {
    top: { x: 0.5, y: 0 },
    bottom: { x: 0.5, y: 1 }
  }
}
const GENERIC_PROCESS_PORTS = {
  top: { x: 0.5, y: 0 },
  right: { x: 1, y: 0.5 },
  bottom: { x: 0.5, y: 1 },
  left: { x: 0, y: 0.5 }
}
const PORT_LINE_LENGTH = 14

const refreshKey = computed(() => `${props.widget.id}-${zoomRefreshTick.value}`)
const deviceStyle = computed(() => ({
  '--device-scale-x': props.widget.type === 'processPump' && props.widget.config.flipX ? -1 : 1,
  '--device-scale-y': props.widget.type === 'processPump' && props.widget.config.flipY ? -1 : 1
}))

const deviceLabel = computed(() => props.widget.type === 'processJunction'
  ? ''
  : (props.widget.config.label || props.widget.config.title || ''))
const hideName = computed(() => Boolean(props.widget.config?.hideName))
const hideJunctionNode = computed(() =>
  props.widget.type === 'processJunction' && Boolean(props.widget.config?.hideNode)
)
const labelStyle = computed(() => ({
  transform: `translate(-50%, 0) translate(${Number(props.widget.config.labelOffsetX) || 0}px, ${Number(props.widget.config.labelOffsetY) || 0}px)`,
  fontSize: `${Math.max(8, Math.min(72, Number(props.widget.config.labelFontSize ?? props.widget.config.titleFontSize) || 12))}px`
}))
function normalizedPortConfig(port, index = 0) {
  const side = ['top', 'right', 'bottom', 'left'].includes(port?.side) ? port.side : null
  if (!side) return null
  const fallback = DEFAULT_PROCESS_PORTS[props.widget.type]?.[side] || GENERIC_PROCESS_PORTS[side]
  const x = Number(port.x)
  const y = Number(port.y)
  return {
    key: String(port.id || `${side}-${index}`),
    side,
    x: Math.max(0, Math.min(1, Number.isFinite(x) ? x : fallback.x)),
    y: Math.max(0, Math.min(1, Number.isFinite(y) ? y : fallback.y))
  }
}
const processPortConfigs = computed(() => {
  if (props.widget.type === 'processJunction' || props.widget.type === 'processValueTag') return []
  const configured = Array.isArray(props.widget.config?.connectionPorts)
    ? props.widget.config.connectionPorts.map(normalizedPortConfig).filter(Boolean)
    : null
  if (configured) return configured
  const defaults = DEFAULT_PROCESS_PORTS[props.widget.type]
  if (!defaults) return []
  return Object.entries(defaults).map(([side, point], index) => normalizedPortConfig({
    id: `default-${side}`,
    side,
    x: point.x,
    y: point.y
  }, index)).filter(Boolean)
})
const portLineSegments = computed(() => processPortConfigs.value.map((port, index) => {
  const x = port.x * 100
  const y = port.y * 100
  const key = `${port.key}-${index}`
  if (port.side === 'left') return { key, x1: x, y1: y, x2: Math.min(100, x + PORT_LINE_LENGTH), y2: y }
  if (port.side === 'right') return { key, x1: x, y1: y, x2: Math.max(0, x - PORT_LINE_LENGTH), y2: y }
  if (port.side === 'top') return { key, x1: x, y1: y, x2: x, y2: Math.min(100, y + PORT_LINE_LENGTH) }
  return { key, x1: x, y1: y, x2: x, y2: Math.max(0, y - PORT_LINE_LENGTH) }
}))
const deviceClass = computed(() => props.widget.type.replace(/[A-Z]/g, m => '-' + m.toLowerCase()))
const dp = computed(() => {
  const id = props.widget.config.varId
  return id == null ? null : store.realtimeData[id]
})

function readBinaryState(value) {
  if (typeof value === 'boolean') return value
  if (typeof value === 'number') return Number.isFinite(value) && value > 0
  const text = String(value ?? '').trim().toLowerCase()
  if (!text) return false
  if (['1', 'true', 'on', 'open', 'run', 'running', 'start', 'yes', '开', '开启', '打开', '运行', '启动'].includes(text)) return true
  if (['0', 'false', 'off', 'close', 'closed', 'stop', 'stopped', 'no', '关', '关闭', '停止'].includes(text)) return false
  const numeric = Number(text)
  return Number.isFinite(numeric) && numeric > 0
}

const isActive = computed(() => {
  if (!dp.value || dp.value.quality !== 'GOOD') return false
  return readBinaryState(dp.value.value)
})
const statusClass = computed(() => {
  if (!props.widget.config.varId) return 'manual'
  if (!dp.value || dp.value.quality !== 'GOOD') return 'offline'
  return isActive.value ? 'active' : 'idle'
})
const fillPercent = computed(() => {
  const raw = Number(props.widget.config.fill)
  return Math.max(8, Math.min(94, Number.isFinite(raw) ? raw : 65))
})
const poolWaterPath = computed(() => {
  const y = 136 - fillPercent.value * 1.05
  return `M14 ${y} C58 ${y - 12} 92 ${y + 12} 136 ${y} S214 ${y - 12} 258 ${y} 334 ${y - 8} 366 ${y} V136H246l-26-15h-46l-26 15H14z`
})
const tankLevelPath = computed(() => {
  const y = 104 - fillPercent.value * 0.72
  return `M23 ${y}c12 8 34 8 46 0v48c0 7-11 13-23 13s-23-6-23-13z`
})

function startLabelDrag(e) {
  if (props.readonly) return
  const startX = e.clientX
  const startY = e.clientY
  const originX = Number(props.widget.config.labelOffsetX) || 0
  const originY = Number(props.widget.config.labelOffsetY) || 0
  const z = Number(canvasZoom.value) || 1

  const onMove = (ev) => {
    const nextConfig = {
      ...props.widget.config,
      labelOffsetX: originX + (ev.clientX - startX) / z,
      labelOffsetY: originY + (ev.clientY - startY) / z
    }
    store.updateWidget(props.widget.id, { config: nextConfig })
  }
  const onUp = () => {
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}
</script>

<style scoped>
.process-device {
  position: relative;
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: visible;
  color: #1f2937;
}
.process-device svg {
  width: 100%;
  height: 100%;
  display: block;
  overflow: visible;
  filter: none;
  shape-rendering: geometricPrecision;
  text-rendering: geometricPrecision;
  transform: scale(var(--device-scale-x, 1), var(--device-scale-y, 1));
  transform-origin: center center;
}
.process-port-lines {
  position: absolute;
  inset: 0;
  z-index: 2;
  pointer-events: none;
}
.process-port-line {
  fill: none;
  stroke: #263544;
  stroke-width: 4;
  stroke-linecap: round;
  vector-effect: non-scaling-stroke;
  transition: stroke 0.18s ease, opacity 0.18s ease;
}
.active .process-port-line {
  stroke: #16a34a;
}
.idle .process-port-line {
  stroke: #dc2626;
}
.offline .process-port-line {
  stroke: #64748b;
}
.device-label {
  position: absolute;
  left: 50%;
  top: -30px;
  z-index: 4;
  max-width: 160%;
  padding: 2px 7px;
  border-radius: var(--radius-sm);
  background: rgba(255, 255, 255, 0.82);
  border: 1px solid rgba(120, 133, 146, 0.28);
  color: var(--text-primary);
  font-size: 12px;
  font-weight: 700;
  white-space: nowrap;
  box-shadow: none;
}
.device-label.draggable {
  cursor: move;
}
.pipe,
.pipe-end-line,
.tank-pipe {
  fill: none;
  stroke: #176e45;
  stroke-width: 5;
  stroke-linecap: round;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.body,
.tank-top,
.tank-bottom {
  stroke: #56636d;
  stroke-width: 2;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.core {
  fill: #f8fafc;
  stroke: #5f6d78;
  stroke-width: 2;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.impeller {
  fill: #6b7780;
  opacity: 0.86;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.base {
  fill: #5a646c;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.state-dot {
  fill: #f2c94c;
  stroke: #ffffff;
  stroke-width: 1.5;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.active .state-dot {
  fill: #20c56b;
  filter: none;
}
.idle .state-dot {
  fill: #ef4444;
  filter: none;
}
.offline .state-dot {
  fill: #94a3b8;
}
.wall {
  fill: rgba(255, 255, 255, 0.12);
  stroke: #24313b;
  stroke-width: 3;
}
.dam-body {
  stroke: #111827;
  stroke-width: 2;
}
.dam-side,
.dam-top,
.dam-bottom {
  fill: none;
  stroke: #111827;
  stroke-width: 2;
}
.water {
  opacity: 0.92;
}
.wave {
  fill: none;
  stroke: rgba(255, 255, 255, 0.84);
  stroke-width: 5;
  stroke-linecap: round;
}
.wave.soft {
  stroke-width: 4;
  opacity: 0.7;
}
.tank-body {
  stroke: #66727b;
  stroke-width: 2;
}
.tank-level {
  fill: rgba(56, 189, 248, 0.45);
}
.mixer-rim {
  fill: #9af4ff;
  stroke: #1287b1;
  stroke-width: 3;
}
.mixer-water {
  fill: rgba(210, 255, 255, 0.44);
  stroke: rgba(255, 255, 255, 0.72);
  stroke-width: 2;
}
.mixer-wall {
  stroke: #126f9f;
  stroke-width: 3;
}
.mixer-bridge,
.mixer-leg {
  fill: none;
  stroke: #e8fbff;
  stroke-width: 10;
  stroke-linecap: round;
}
.mixer-hub {
  fill: #d9fbff;
  stroke: #76d7e4;
  stroke-width: 2;
}
.dosing-pipe,
.dosing-shaft,
.dosing-impeller {
  fill: none;
  stroke: #273544;
  stroke-width: 3;
  stroke-linecap: round;
  stroke-linejoin: round;
  transition: stroke 0.18s ease;
}
.dosing-pipe {
  stroke: #176e45;
  stroke-width: 4;
}
.dosing-frame {
  fill: rgba(255, 255, 255, 0.38);
  stroke: #273544;
  stroke-width: 2;
}
.dosing-motor {
  fill: #e2e8ee;
  stroke: #273544;
  stroke-width: 2;
  transition: fill 0.18s ease, stroke 0.18s ease;
}
.dosing-hub {
  fill: #f8fafc;
  stroke: #273544;
  stroke-width: 2;
}
.process-dosing-machine.active .dosing-motor {
  fill: #22c55e;
  stroke: #087a42;
}
.process-dosing-machine.active .dosing-pipe,
.process-dosing-machine.active .dosing-shaft,
.process-dosing-machine.active .dosing-impeller {
  stroke: #16a34a;
}
.process-dosing-machine.idle .dosing-motor {
  fill: #ef4444;
  stroke: #991b1b;
}
.process-dosing-machine.idle .dosing-pipe,
.process-dosing-machine.idle .dosing-shaft,
.process-dosing-machine.idle .dosing-impeller {
  stroke: #dc2626;
}
.process-dosing-machine.offline .dosing-motor {
  fill: #bdc7ce;
  stroke: #64748b;
}
.junction-node {
  fill: #178447;
  stroke: #ffffff;
  stroke-width: 2;
  vector-effect: non-scaling-stroke;
}
.junction-svg-hidden {
  opacity: 0;
}
.valve-body {
  fill: #21d65f;
  stroke: #1d2936;
  stroke-width: 2;
  stroke-linejoin: round;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.process-pump.active .body {
  fill: #20c56b;
  stroke: #087a42;
}
.process-pump.idle .body {
  fill: #ef4444;
  stroke: #991b1b;
}
.process-pump.active .pipe {
  stroke: #16a34a;
}
.process-pump.idle .pipe {
  stroke: #dc2626;
}
.process-pump.active .core {
  stroke: #087a42;
}
.process-pump.idle .core {
  stroke: #991b1b;
}
.process-pump.active .impeller,
.process-pump.idle .impeller {
  fill: rgba(255, 255, 255, 0.82);
}
.process-valve.active .valve-body,
.process-valve-vertical.active .valve-body,
.process-valve.active .valve-handle,
.process-valve-vertical.active .valve-handle {
  fill: #22c55e;
}
.process-valve.idle .valve-body,
.process-valve-vertical.idle .valve-body,
.process-valve.idle .valve-handle,
.process-valve-vertical.idle .valve-handle {
  fill: #ef4444;
}
.process-valve.idle .pipe,
.process-valve-vertical.idle .pipe {
  stroke: #dc2626;
}
.process-valve.active .pipe,
.process-valve-vertical.active .pipe {
  stroke: #16a34a;
}
.offline .valve-body,
.offline .body {
  fill: #bdc7ce;
}
.valve-stem {
  fill: #d9dde2;
  stroke: #1d2936;
  stroke-width: 1.5;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.valve-handle {
  fill: #23d967;
  stroke: #1d2936;
  stroke-width: 1.5;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.pipe-end-line {
  stroke: #18b8bf;
}
.pipe-end-head {
  fill: #37dce5;
  stroke: #0e7b82;
  stroke-width: 2;
}
.calciner-pipe,
.cyclone-pipe,
.preheater-pipe,
.filter-duct,
.fan-pipe,
.screw-pipe,
.elevator-inlet,
.elevator-outlet,
.heat-pipe,
.kiln-pipe,
.airlock-port,
.damper-duct,
.spray-line,
.flap-port,
.chute-inlet,
.chute-outlet {
  fill: none;
  stroke: #263544;
  stroke-width: 4;
  stroke-linecap: round;
}
.calciner-shell,
.calciner-top,
.calciner-cone,
.cyclone-body,
.cyclone-top,
.cyclone-cone,
.preheater-shell,
.filter-house,
.filter-hopper,
.fan-scroll,
.silo-shell,
.silo-top,
.silo-cone,
.screw-trough,
.elevator-case,
.gas-skid-frame,
.status-panel,
.heat-shell,
.kiln-shell,
.kiln-end,
.airlock-body,
.damper-frame,
.spray-body,
.spray-nozzle,
.chute-body,
.flap-shell {
  stroke: #263544;
  stroke-width: 2;
}
.calciner-top,
.cyclone-top,
.preheater-shell,
.filter-house,
.fan-scroll,
.screw-trough,
.elevator-case,
.gas-skid-frame,
.status-panel,
.heat-shell,
.kiln-shell,
.kiln-end,
.airlock-body,
.damper-frame,
.spray-body,
.spray-nozzle,
.flap-shell {
  fill: #dce3e8;
}
.calciner-band,
.preheater-band,
.filter-roof,
.filter-bags,
.filter-outlet,
.elevator-belt,
.heat-band,
.heat-tube-bank,
.kiln-ribs,
.stack-bands,
.damper-shaft,
.flap-divider {
  fill: none;
  stroke: #6b7780;
  stroke-width: 2;
  stroke-linecap: round;
}
.calciner-bed {
  fill: rgba(245, 158, 11, 0.55);
  stroke: #a16207;
  stroke-width: 1.5;
}
.cyclone-vortex,
.preheater-coil,
.heat-coil {
  fill: none;
  stroke: #64748b;
  stroke-width: 2.5;
  stroke-linecap: round;
  stroke-linejoin: round;
}
.filter-hopper {
  fill: #cbd5dc;
}
.filter-indicators circle {
  fill: #ef4444;
  stroke: #f8fafc;
  stroke-width: 1.2;
}
.heat-coil {
  stroke-width: 3;
}
.kiln-shell {
  fill: #cbd5dc;
}
.kiln-end {
  fill: #e8eef2;
}
.kiln-rollers {
  fill: #64748b;
  stroke: #263544;
  stroke-width: 1.5;
}
.kiln-drive {
  fill: #eef2f5;
  stroke: #263544;
  stroke-width: 2;
}
.stack-top,
.stack-body {
  stroke: #263544;
  stroke-width: 2;
}
.stack-base {
  fill: #64748b;
  stroke: #263544;
  stroke-width: 2;
}
.stack-smoke {
  fill: none;
  stroke: #94a3b8;
  stroke-width: 3;
  stroke-linecap: round;
  opacity: 0.75;
}
.airlock-body {
  fill: #dbe4ea;
}
.airlock-star {
  fill: #22c55e;
  stroke: #263544;
  stroke-width: 1.8;
  stroke-linejoin: round;
}
.damper-blade {
  fill: none;
  stroke: #22c55e;
  stroke-width: 5;
  stroke-linecap: round;
}
.damper-actuator,
.flap-actuator {
  fill: #22c55e;
  stroke: #263544;
  stroke-width: 2;
}
.spray-body,
.spray-nozzle {
  fill: #dbe4ea;
}
.spray-fan {
  fill: none;
  stroke: #f97316;
  stroke-width: 3;
  stroke-linecap: round;
}
.chute-body {
  fill: #cbd5dc;
}
.chute-liner {
  fill: none;
  stroke: #64748b;
  stroke-width: 2;
  stroke-linecap: round;
}
.flap-plate {
  fill: none;
  stroke: #22c55e;
  stroke-width: 4;
  stroke-linecap: round;
}
.fan-scroll {
  fill: #cfd8de;
}
.fan-wheel,
.screw-drive,
.elevator-pulley,
.gas-gauge {
  fill: #eef2f5;
  stroke: #263544;
  stroke-width: 2;
}
.fan-blades {
  fill: #6b7780;
}
.fan-base {
  fill: #5f6b75;
}
.wincc-fan-port,
.wincc-fan-motor-link {
  fill: none;
  stroke: #2ab40e;
  stroke-width: 4;
  stroke-linecap: round;
  stroke-linejoin: round;
  transition: stroke 0.18s ease, opacity 0.18s ease;
}
.wincc-fan-body,
.wincc-fan-ring,
.wincc-fan-core,
.wincc-fan-base,
.wincc-fan-motor {
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.wincc-fan-body {
  stroke: #17770d;
  stroke-width: 3;
}
.wincc-fan-ring {
  fill: none;
  stroke: #21a817;
  stroke-width: 5;
}
.wincc-fan-core {
  fill: #7cf02b;
  stroke: #14900f;
  stroke-width: 2.2;
}
.wincc-fan-blades {
  fill: #3fc61a;
  stroke: #0f7f11;
  stroke-width: 1.7;
  stroke-linejoin: round;
  transition: stroke 0.18s ease, fill 0.18s ease, opacity 0.18s ease;
}
.wincc-fan-base {
  fill: #4dd817;
  stroke: #17770d;
  stroke-width: 2;
  stroke-linejoin: round;
}
.wincc-fan-motor {
  fill: #f8fafc;
  stroke: #66727b;
  stroke-width: 1.5;
}
.wincc-fan-motor-text {
  fill: #4b5563;
  font-size: 10px;
  font-weight: 800;
  font-family: Arial, sans-serif;
  text-anchor: middle;
  dominant-baseline: central;
}
.burner-gas-line,
.gas-line {
  fill: none;
  stroke: #d99a1a;
  stroke-width: 6;
  stroke-linecap: round;
}
.burner-body,
.burner-nozzle {
  fill: #d8e0e6;
  stroke: #263544;
  stroke-width: 2;
}
.burner-flame.outer {
  fill: #f97316;
  stroke: #b45309;
  stroke-width: 1.5;
}
.burner-flame.inner {
  fill: #fde047;
}
.silo-fill {
  fill: rgba(241, 245, 249, 0.86);
  stroke: rgba(148, 163, 184, 0.45);
  stroke-width: 1;
}
.screw-flight {
  fill: none;
  stroke: #475569;
  stroke-width: 4;
  stroke-linecap: round;
}
.elevator-buckets {
  fill: #94a3b8;
  stroke: #263544;
  stroke-width: 1.5;
}
.gas-valve {
  fill: #22c55e;
  stroke: #263544;
  stroke-width: 2;
}
.gas-valve-handle,
.gas-gauge-needle {
  fill: none;
  stroke: #263544;
  stroke-width: 2.2;
  stroke-linecap: round;
}
.status-panel {
  fill: rgba(226, 232, 240, 0.92);
}
.status-title,
.status-row text {
  fill: #1f2937;
  font-size: 12px;
  font-weight: 700;
  font-family: Arial, sans-serif;
}
.status-title {
  font-size: 11px;
}
.lamp-ok,
.lamp-alarm,
.lamp-idle {
  stroke: #ffffff;
  stroke-width: 1.5;
}
.lamp-ok {
  fill: #22c55e;
}
.lamp-alarm {
  fill: #ef4444;
}
.lamp-idle {
  fill: #f2c94c;
}
.process-calciner-furnace.active .calciner-bed,
.process-gas-valve-skid.active .gas-valve,
.process-fan-blower.active .fan-wheel,
.process-wincc-fan.active .wincc-fan-body,
.process-wincc-fan.active .wincc-fan-base,
.process-wincc-fan.active .wincc-fan-core,
.process-screw-conveyor.active .screw-flight,
.process-bucket-elevator.active .elevator-buckets,
.process-bag-filter.active .filter-bags,
.process-preheater-tower.active .preheater-coil,
.process-cyclone-separator.active .cyclone-vortex,
.process-heat-exchanger.active .heat-coil,
.process-kiln-tube.active .kiln-drive,
.process-rotary-airlock.active .airlock-star,
.process-damper.active .damper-blade,
.process-spray-lance.active .spray-fan,
.process-double-flap-valve.active .flap-plate {
  fill: #22c55e;
  stroke: #087a42;
}
.process-wincc-fan.active .wincc-fan-port,
.process-wincc-fan.active .wincc-fan-motor-link {
  stroke: #16a34a;
}
.process-wincc-fan.active .wincc-fan-ring,
.process-wincc-fan.active .wincc-fan-blades {
  stroke: #087a42;
}
.process-spray-lance.active .spray-fan {
  fill: none;
  stroke: #f97316;
}
.process-heat-exchanger.active .heat-coil,
.process-damper.active .damper-blade,
.process-double-flap-valve.active .flap-plate {
  fill: none;
}
.process-burner.active .burner-flame.inner {
  fill: #fde047;
  stroke: none;
}
.process-fan-blower.active .fan-blades,
.process-wincc-fan.active .wincc-fan-blades,
.process-screw-conveyor.active .screw-drive,
.process-bucket-elevator.active .elevator-pulley {
  fill: #22c55e;
}
.process-calciner-furnace.idle .calciner-bed,
.process-gas-valve-skid.idle .gas-valve,
.process-fan-blower.idle .fan-wheel,
.process-wincc-fan.idle .wincc-fan-body,
.process-wincc-fan.idle .wincc-fan-base,
.process-wincc-fan.idle .wincc-fan-core,
.process-screw-conveyor.idle .screw-flight,
.process-bucket-elevator.idle .elevator-buckets,
.process-bag-filter.idle .filter-bags,
.process-preheater-tower.idle .preheater-coil,
.process-cyclone-separator.idle .cyclone-vortex,
.process-heat-exchanger.idle .heat-coil,
.process-kiln-tube.idle .kiln-drive,
.process-rotary-airlock.idle .airlock-star,
.process-damper.idle .damper-blade,
.process-spray-lance.idle .spray-fan,
.process-double-flap-valve.idle .flap-plate {
  fill: #ef4444;
  stroke: #991b1b;
}
.process-wincc-fan.idle .wincc-fan-port,
.process-wincc-fan.idle .wincc-fan-motor-link {
  stroke: #dc2626;
}
.process-wincc-fan.idle .wincc-fan-ring,
.process-wincc-fan.idle .wincc-fan-blades {
  stroke: #991b1b;
}
.process-heat-exchanger.idle .heat-coil,
.process-damper.idle .damper-blade,
.process-spray-lance.idle .spray-fan,
.process-double-flap-valve.idle .flap-plate {
  fill: none;
}
.process-burner.idle .burner-flame {
  fill: #94a3b8;
  stroke: #64748b;
  opacity: 0.45;
}
.process-fan-blower.idle .fan-blades,
.process-wincc-fan.idle .wincc-fan-blades,
.process-screw-conveyor.idle .screw-drive,
.process-bucket-elevator.idle .elevator-pulley {
  fill: #ef4444;
}
.offline .calciner-bed,
.offline .fan-wheel,
.offline .wincc-fan-body,
.offline .wincc-fan-base,
.offline .wincc-fan-core,
.offline .gas-valve,
.offline .screw-drive,
.offline .elevator-buckets,
.offline .filter-bags,
.offline .preheater-coil,
.offline .cyclone-vortex,
.offline .heat-coil,
.offline .kiln-drive,
.offline .airlock-star,
.offline .damper-blade,
.offline .spray-fan,
.offline .flap-plate {
  fill: #bdc7ce;
  stroke: #64748b;
}
.offline .wincc-fan-port,
.offline .wincc-fan-motor-link,
.offline .wincc-fan-ring,
.offline .wincc-fan-blades {
  stroke: #64748b;
}
.offline .wincc-fan-blades {
  fill: #94a3b8;
}
.offline .heat-coil,
.offline .damper-blade,
.offline .spray-fan,
.offline .flap-plate {
  fill: none;
}
</style>
