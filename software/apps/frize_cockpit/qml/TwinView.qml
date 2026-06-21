import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import QtQuick3D.AssetUtils
import Frize.Cockpit

// 실시간 3D 디지털 트윈 ― 드론 LiDAR + 고글 열화상이 융합된 TSDF 표면 메쉬.
//
// 매핑 서비스(frize_mapping)가 TSDF→마칭큐브로 구운 twin.gltf 를 RuntimeLoader가
// 런타임 로드해 '매끈한 표면(정점=열화상 컬러)'으로 렌더한다. 파일이 갱신될 때마다
// cockpit.twinMeshUrl 이 바뀌어 자동 리로드. 메쉬가 아직 없으면 점유 복셀로 폴백.
// 미탐사 영역은 안개 + 피사계심도로 흐리게 → 드론이 그쪽을 채워간다.
Item {
    id: root
    property real meterToScene: 1.0          // glTF는 실척(m) → 1 unit = 1 m
    readonly property bool hasMesh: cockpit.twinMeshUrl && cockpit.twinMeshUrl.length > 0
    // 화재 전선/확산 마커가 살아있게 보이도록 공용 펄스(0→1 반복)
    property real firePulse: 0.0
    NumberAnimation on firePulse {
        from: 0.0; to: 1.0; duration: 900; loops: Animation.Infinite; running: true
    }

    View3D {
        id: view
        anchors.fill: parent
        environment: SceneEnvironment {
            clearColor: "#0c0f13"; backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA; antialiasingQuality: SceneEnvironment.High
            // 접촉음영(AO) → 방 모서리·가구가 입체적으로 읽힘
            aoEnabled: true; aoStrength: 75; aoDistance: 1.2; aoSoftness: 30
            tonemapMode: SceneEnvironment.TonemapModeFilmic
            // 미탐사 영역 흐리게: 거리 안개 + 피사계심도
            fog: Fog {
                enabled: true; color: "#0c0f13"; density: 0.32
                depthEnabled: true; depthNear: 20; depthFar: 65
            }
            depthOfFieldEnabled: true
            depthOfFieldFocusDistance: 24
            depthOfFieldFocusRange: 22
            depthOfFieldBlurAmount: 2.4
        }

        PerspectiveCamera {
            id: cam; clipFar: 220
            position: Qt.vector3d(18, 22, 18); eulerRotation: Qt.vector3d(-38, 45, 0)
        }
        DirectionalLight { eulerRotation: Qt.vector3d(-55, -30, 0); brightness: 1.15; castsShadow: true }
        DirectionalLight { eulerRotation: Qt.vector3d(-20, 140, 0); brightness: 0.45; color: "#9fc0ff" }
        DirectionalLight { eulerRotation: Qt.vector3d(90, 0, 0); brightness: 0.25; color: "#ffb27a" } // 화점 반사 채움

        // 바닥 그리드
        Model {
            source: "#Rectangle"; eulerRotation: Qt.vector3d(-90,0,0); scale: Qt.vector3d(2,2,1)
            materials: PrincipledMaterial { baseColor: "#13171d"; roughness: 1.0 }
        }
        AxisHelper { enableXYGrid: false; enableXZGrid: true; gridColor: "#222a33"; scale: Qt.vector3d(0.5,0.5,0.5) }

        // ── TSDF 표면 메쉬(주역): glTF 런타임 로드 ────────────────────────────
        // ENU(z-up) → Qt(y-up): X축 -90° 회전으로 (x,y,z)→(x,z,-y) 매핑.
        Node {
            visible: root.hasMesh
            eulerRotation: Qt.vector3d(-90, 0, 0)
            scale: Qt.vector3d(root.meterToScene, root.meterToScene, root.meterToScene)
            RuntimeLoader {
                id: twinMesh
                source: cockpit.twinMeshUrl
                // glTF가 정점컬러(COLOR_0=열화상)를 들고 오므로 그대로 PBR로 표시.
                instancing: null
            }
        }

        // ── 점유 복셀(폴백/디테일): 메쉬 없을 때만 큐브로 표시 ─────────────────
        Repeater3D {
            model: root.hasMesh ? [] : cockpit.twinVoxels
            delegate: Model {
                source: "#Cube"
                position: Qt.vector3d(modelData.x*root.meterToScene, modelData.z*root.meterToScene, -modelData.y*root.meterToScene)
                scale: Qt.vector3d(cockpit.voxelSize*0.0105, cockpit.voxelSize*0.0105, cockpit.voxelSize*0.0105)
                materials: PrincipledMaterial {
                    property real t: modelData.t
                    baseColor: t > -100 ? Qt.rgba(0.9, 0.35 - Math.min(t,300)/900, 0.15, 1) : "#3a424d"
                    emissiveFactor: t > 60 ? Qt.vector3d(0.6,0.15,0.05) : Qt.vector3d(0,0,0)
                    roughness: 0.8
                }
            }
        }

        // 위험구역(반투명 발광 구)
        Repeater3D {
            model: cockpit.hazardZones
            delegate: Model {
                source: "#Sphere"
                position: Qt.vector3d(modelData.x, 1.0, -modelData.y)
                scale: Qt.vector3d(modelData.radius*0.02, modelData.radius*0.02, modelData.radius*0.02)
                opacity: 0.26
                materials: PrincipledMaterial {
                    baseColor: modelData.severity==="critical" ? "#e0241c" : modelData.severity==="high" ? "#ff8c00" : "#f5c518"
                    emissiveFactor: Qt.vector3d(0.4,0.1,0.05); alphaMode: PrincipledMaterial.Blend
                }
            }
        }

        // 프런티어(미탐사 경계 — 드론 탐사 목표) : 작은 시안 마커
        Repeater3D {
            model: cockpit.twinFrontiers
            delegate: Model {
                source: "#Sphere"
                position: Qt.vector3d(modelData.x, modelData.z, -modelData.y)
                scale: Qt.vector3d(0.0016,0.0016,0.0016)
                materials: PrincipledMaterial { baseColor: "#35c4e0"; emissiveFactor: Qt.vector3d(0.1,0.4,0.5) }
            }
        }

        // ── 화재 확산 전선(번지는 가장자리): 지금 막 달궈지는 셀들 ──────────────
        // 가열률(rate)이 클수록 밝고 크게 펄스 → '여기가 지금 번지는 중'을 강조.
        Repeater3D {
            model: cockpit.fireFront
            delegate: Model {
                source: "#Sphere"
                // ENU(z-up) → Qt(y-up): (x,y,z) → (x, z, -y)
                position: Qt.vector3d(modelData.x, modelData.z, -modelData.y)
                property real hot: Math.min(1.0, modelData.rate/12.0)   // 0..1 가열강도
                property real s: 0.003 + 0.004*hot + 0.0015*root.firePulse*hot
                scale: Qt.vector3d(s, s, s)
                materials: PrincipledMaterial {
                    baseColor: Qt.rgba(1.0, 0.45 - 0.3*hot, 0.1, 1.0)
                    // 펄스 + 가열강도로 발광 → 활발한 전선일수록 더 이글거림
                    emissiveFactor: Qt.vector3d(1.0*(0.4+0.6*root.firePulse)*(0.5+hot),
                                                0.35*(0.4+0.6*root.firePulse)*hot, 0.05)
                    roughness: 0.6
                }
            }
        }

        // ── 화재 확산 벡터(어디로 번지나): 화점 중심 → 진행방향 코멧 트레일 ────────
        Repeater3D {
            model: cockpit.fireSpread
            delegate: Node {
                id: spreadNode
                property var sv: modelData
                property bool moving: sv.rate > 0.05
                // 화점 핵: 크고 붉게 이글거리는 구
                Model {
                    source: "#Sphere"
                    position: Qt.vector3d(spreadNode.sv.cx, spreadNode.sv.cz, -spreadNode.sv.cy)
                    property real s: 0.006 + 0.006*Math.min(1.0, spreadNode.sv.peak/600.0)
                    scale: Qt.vector3d(s, s, s)
                    materials: PrincipledMaterial {
                        baseColor: "#ff3b14"
                        emissiveFactor: Qt.vector3d(1.0, 0.25, 0.05); roughness: 0.5
                    }
                }
                // 진행방향 화살촉 트레일: 중심에서 dir 방향으로 점점 작아지는 점들
                Repeater3D {
                    model: spreadNode.moving ? 5 : 0
                    delegate: Model {
                        source: "#Sphere"
                        property real step: (index+1) * 0.55
                        position: Qt.vector3d(spreadNode.sv.cx + spreadNode.sv.dx*step,
                                              spreadNode.sv.cz + spreadNode.sv.dz*step,
                                              -(spreadNode.sv.cy + spreadNode.sv.dy*step))
                        property real s: 0.006 - index*0.0009
                        scale: Qt.vector3d(s, s, s)
                        materials: PrincipledMaterial {
                            // 진행할수록 주황→노랑(앞으로 번질 쪽을 미리 보여줌)
                            baseColor: Qt.rgba(1.0, 0.5+index*0.08, 0.1, 1.0)
                            emissiveFactor: Qt.vector3d(0.9-index*0.12, 0.4+index*0.06, 0.05)
                            opacity: 0.85 - index*0.12; alphaMode: PrincipledMaterial.Blend
                        }
                    }
                }
            }
        }

        // ── 드론 재정찰 목표: '되돌아가 확인할' 화재 구역(시안 펄스 링) ───────────
        Repeater3D {
            model: cockpit.revisitTargets
            delegate: Model {
                source: "#Sphere"
                position: Qt.vector3d(modelData.x, modelData.z, -modelData.y)
                property real s: 0.007 + 0.003*root.firePulse
                scale: Qt.vector3d(s, s, s)
                opacity: 0.30;
                materials: PrincipledMaterial {
                    baseColor: "#2fe0d0"; emissiveFactor: Qt.vector3d(0.1,0.5,0.45)
                    alphaMode: PrincipledMaterial.Blend
                }
            }
        }

        // 디바이스 마커
        Repeater3D {
            model: cockpit.devices
            delegate: Node {
                position: Qt.vector3d(model.posX, 1.2, -model.posY)
                Model {
                    source: model.deviceType==="scout" ? "#Cone" : "#Cylinder"
                    scale: Qt.vector3d(0.012,0.02,0.012)
                    materials: PrincipledMaterial {
                        baseColor: model.deviceType==="scout" ? "#35c4e0" : model.deviceType==="apparatus" ? "#b8923a" : "#e0241c"
                        emissiveFactor: Qt.vector3d(0.15,0.15,0.15)
                    }
                }
            }
        }

        OrbitCameraController { origin: originNode; camera: cam }
        Node { id: originNode }
    }

    // HUD: 탐사 진행 + 트윈 소스 표시
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 12
        width: hud.implicitWidth+20; height: 26; radius: 4
        color: Qt.rgba(0,0,0,0.5)
        Row { id: hud; anchors.centerIn: parent; spacing: 8
            Text { text: "DIGITAL TWIN"; color: "#cfd4da"; font.pixelSize: 11; font.bold: true; font.family: "Consolas" }
            Text { text: (root.hasMesh ? "· TSDF surface" : "· voxel " + cockpit.twinVoxels.length)
                        + " · frontier " + cockpit.twinFrontiers.length
                   color: "#7c828c"; font.pixelSize: 11; font.family: "Consolas" }
            Text {
                visible: cockpit.fireFront.length > 0 || cockpit.fireSpread.length > 0
                text: "· 🔥 spread front " + cockpit.fireFront.length
                      + " · fire " + cockpit.fireSpread.length
                      + " · revisit " + cockpit.revisitTargets.length
                color: "#ff7a3c"; font.pixelSize: 11; font.bold: true; font.family: "Consolas"
            }
        }
    }
}
