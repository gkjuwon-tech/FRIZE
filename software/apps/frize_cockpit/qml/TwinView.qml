import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import Frize.Cockpit

// 실시간 3D 디지털 트윈 ― 드론 LiDAR + 고글 열화상이 융합된 점유/열 복셀맵.
// 미탐사(빈) 영역은 안개 + 피사계심도로 흐리게(blur) 표시 → 드론이 그쪽을 채워간다.
Item {
    id: root
    property real meterToScene: 1.0

    View3D {
        id: view
        anchors.fill: parent
        environment: SceneEnvironment {
            clearColor: "#0c0f13"; backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA; antialiasingQuality: SceneEnvironment.High
            // 미탐사 영역 흐리게: 거리 안개 + 피사계심도
            fog: Fog {
                enabled: true; color: "#0c0f13"; density: 0.35
                depthEnabled: true; depthNear: 18; depthFar: 60
            }
            depthOfFieldEnabled: true
            depthOfFieldFocusDistance: 22
            depthOfFieldFocusRange: 18
            depthOfFieldBlurAmount: 3.0
        }

        PerspectiveCamera {
            id: cam; clipFar: 200
            position: Qt.vector3d(18, 22, 18); eulerRotation: Qt.vector3d(-38, 45, 0)
        }
        DirectionalLight { eulerRotation: Qt.vector3d(-55, -30, 0); brightness: 1.1 }
        DirectionalLight { eulerRotation: Qt.vector3d(-20, 140, 0); brightness: 0.4; color: "#9fc0ff" }

        // 바닥 그리드
        Model {
            source: "#Rectangle"; eulerRotation: Qt.vector3d(-90,0,0); scale: Qt.vector3d(2,2,1)
            materials: PrincipledMaterial { baseColor: "#13171d"; roughness: 1.0 }
        }
        AxisHelper { enableXYGrid: false; enableXZGrid: true; gridColor: "#222a33"; scale: Qt.vector3d(0.5,0.5,0.5) }

        // 점유 복셀(열에 따라 색): 드론+고글 융합 결과
        Repeater3D {
            model: cockpit.twinVoxels
            delegate: Model {
                source: "#Cube"
                position: Qt.vector3d(modelData.x*root.meterToScene, modelData.z*root.meterToScene, -modelData.y*root.meterToScene)
                scale: Qt.vector3d(cockpit.voxelSize*0.0105, cockpit.voxelSize*0.0105, cockpit.voxelSize*0.0105)
                materials: PrincipledMaterial {
                    // 온도 있으면 적색 발광, 없으면 차가운 회색
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
                opacity: 0.28
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

    // HUD: 탐사 진행
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 12
        width: hud.implicitWidth+20; height: 26; radius: 4
        color: Qt.rgba(0,0,0,0.5)
        Row { id: hud; anchors.centerIn: parent; spacing: 8
            Text { text: "DIGITAL TWIN"; color: "#cfd4da"; font.pixelSize: 11; font.bold: true; font.family: "Consolas" }
            Text { text: "· voxels " + cockpit.twinVoxels.length + " · frontier " + cockpit.twinFrontiers.length
                   color: "#7c828c"; font.pixelSize: 11; font.family: "Consolas" }
        }
    }
}
