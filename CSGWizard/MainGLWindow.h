#ifndef MAINGLWINDOW_H
#define MAINGLWINDOW_H

#include <QResizeEvent>
#include "GLWindow.h"
#include "Camera.h"
#include "GLMesh.h"

class MainGLWindow : public GLWindow
{
    Q_OBJECT
protected:
    Camera m_Camera;

    Ogre::Vector3 m_TrackballCenter;
    float m_DistToCenter;
    float m_CamXRot;
    float m_CamYRot;

    QPoint m_MousePos;
    Ogre::Vector3 m_LastIntersectionPos;

    void updateCameraPos();

    std::shared_ptr<GLMesh> m_Mesh;

    std::shared_ptr<OctreeSF> m_RaycastOctree;

    BVHScene m_CollisionGeometry;

    virtual void initializeGL() override;
    virtual void render() override;

    virtual void resizeEvent(QResizeEvent* event) override;

    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;

    bool raycast(QMouseEvent* event, Ogre::Vector3& rayIntersection);

public:
    MainGLWindow();
};

#endif // MAINGLWINDOW_H