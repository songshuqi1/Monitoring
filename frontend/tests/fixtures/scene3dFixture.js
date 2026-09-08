import { createSceneNode, emptyScene } from '../../src/services/scene3dService.js'

// Test-only scene used to exercise layouts, bindings, export and camera behavior.
export function fixtureScene() {
  const scene = emptyScene()
  scene.nodes = [
    createSceneNode('platform', { name: '供水设备区', position: [0, -0.12, 0], scale: [2.6, 1, 1.8], locked: true }),
    createSceneNode('tank', { name: '原水储罐 T-101', deviceId: 'T-101', position: [-5, 0, -2] }),
    createSceneNode('tank', { name: '清水储罐 T-102', deviceId: 'T-102', position: [5, 0, -2] }),
    createSceneNode('pump', { name: '输送泵 P-101', deviceId: 'P-101', position: [-2.4, 0, 2.5] }),
    createSceneNode('pump', { name: '备用泵 P-102', deviceId: 'P-102', position: [2.4, 0, 2.5] }),
    createSceneNode('valve', { name: '进水阀 V-101', position: [0, 0, -2] }),
    createSceneNode('pipe', { name: '原水管段', position: [-2.5, 0, -2], pipeLength: 3 }),
    createSceneNode('pipe', { name: '出水管段', position: [2.5, 0, -2], pipeLength: 3 }),
    createSceneNode('pipe', { name: '泵组总管', position: [0, 0, 4], pipeLength: 8 }),
    createSceneNode('fan', { name: '通风机 F-101', position: [7, 0, 3] })
  ]
  return scene
}
