/*
 * Copyright (C) 2019 Deepin Technology Co., Ltd.
 *
 * Author:     Sian Cao <yinshuiboy@gmail.com>
 *
 * Maintainer: Sian Cao <yinshuiboy@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _DEEPIN_MULTITASKING_H
#define _DEEPIN_MULTITASKING_H 

#include <QObject>
#include <QtWidgets>
#include <QQuickView>
#include <QQuickPaintedItem>
#include <QQuickWidget>
#include <QWindow>
#include <QTimeLine>
#include <kwineffects.h>
#include <KF5/KWindowSystem/KWindowSystem>

#include "background.h"

using namespace KWin;

class DesktopThumbnail: public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
    Q_PROPERTY(float radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(QVariant windows READ windows NOTIFY windowsChanged)
public:
    DesktopThumbnail(QQuickItem* parent = 0): QQuickPaintedItem(parent) {
        setRenderTarget(QQuickPaintedItem::FramebufferObject);
    }

    int desktop() const { return m_desktop; }
    void setDesktop(int d) { 
        if (m_desktop != d) {
            m_desktop = d;

            if (!size().isEmpty()) {
                const auto& pm = BackgroundManager::instance().getBackground(m_desktop, 0);
                m_bg = pm.scaled(size().toSize(), Qt::KeepAspectRatioByExpanding);
            }
            emit desktopChanged(); 

            QList<WId> vl;
            for (auto wid: KWindowSystem::self()->windows()) {
                KWindowInfo info(wid, NET::WMDesktop);
                if (info.valid() && info.desktop() == d) {
                    vl.append(wid);
                }
            }
            setWindows(vl);

            update();
        }
    }

    float radius() const { return m_radius; }
    void setRadius(float d) { 
        if (m_radius != d) {
            m_radius = d;
            emit radiusChanged(); 
        }
    }

    QVariant windows() const { return QVariant::fromValue(m_windows); }
    void setWindows(QList<WId> ids) {
        m_windows.clear();
        for (auto id: ids) {
            m_windows.append(id);
        }
        emit windowsChanged();
    }


    void paint(QPainter* p) override {
        QRect rect(0, 0, width(), height());

        QPainterPath clip;
        clip.addRoundedRect(rect, m_radius, m_radius);
        p->setClipPath(clip);

        p->drawPixmap(0, 0, m_bg);
    }

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override {
        if (!size().isEmpty()) {
            const auto& pm = BackgroundManager::instance().getBackground(m_desktop, 0);
            m_bg = pm.scaled(size().toSize(), Qt::KeepAspectRatioByExpanding);
            update();
        }

        QQuickPaintedItem::geometryChanged(newGeometry, oldGeometry);
    }

signals:
    void desktopChanged();
    void radiusChanged();
    void windowsChanged();

private:
    int m_desktop {0};
    float m_radius {0};
    QVariantList m_windows;
    QPixmap m_bg;
};

//manage DesktopThumbnails
class DesktopThumbnailManager: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QSize thumbSize READ thumbSize NOTIFY thumbSizeChanged)
    Q_PROPERTY(int currentDesktop READ currentDesktop NOTIFY currentDesktopChanged)
    Q_PROPERTY(int desktopCount READ desktopCount NOTIFY desktopCountChanged)
    Q_PROPERTY(bool showPlusButton READ showPlusButton NOTIFY showPlusButtonChanged)
    Q_PROPERTY(QSize containerSize READ containerSize NOTIFY containerSizeChanged)

public:
    DesktopThumbnailManager(EffectsHandler* h);
    void windowInputMouseEvent(QMouseEvent* e);
    void setEnabled(bool v);
    void setEffectWindow(EffectWindow* w) {
        m_effectWindow = w;
    }

    EffectWindow* effectWindow() {
        return m_effectWindow;
    }

    QSize thumbSize() const;
    int desktopCount() const;
    int currentDesktop() const;
    bool showPlusButton() const;
    QSize containerSize() const;

    Q_INVOKABLE QRect calculateDesktopThumbRect(int index);
    Q_INVOKABLE QVariantList windowsFor(int desktop);

protected slots:
    void onDesktopsChanged();

protected:
    void mouseMoveEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* re) override;

signals:
    void currentDesktopChanged();
    void desktopCountChanged();
    void showPlusButtonChanged();
    void containerSizeChanged();
    void thumbSizeChanged();
    void layoutChanged();
    void desktopRemoved(QVariant id);

    // internal
    void switchDesktop(int left, int right);
    void requestChangeCurrentDesktop(int d);
    void requestAppendDesktop();
    void requestDeleteDesktop(int);

private:
    EffectWindow* m_effectWindow {nullptr};
    EffectsHandler* m_handler {nullptr};

    QList<QWidget*> m_thumbs;

    QQuickWidget* m_view {nullptr};

    mutable QSize m_wsThumbSize;

    QSize calculateThumbDesktopSize() const;
};

/**
 *  Deepin Multitasking View Effect
 **/
class MultitaskingEffect : public Effect
{
    Q_OBJECT
public:
    MultitaskingEffect();
    virtual ~MultitaskingEffect();

    virtual void reconfigure(ReconfigureFlags) override;

    // Screen painting
    virtual void prePaintScreen(ScreenPrePaintData &data, int time) override;
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData &data) override;
    virtual void postPaintScreen() override;

    // Window painting
    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    virtual void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    // User interaction
    virtual void windowInputMouseEvent(QEvent *e) override;
    virtual void grabbedKeyboardEvent(QKeyEvent *e) override;
    virtual bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 10;
    }

    void updateHighlightWindow(EffectWindow* w);

public Q_SLOTS:
    void setActive(bool active);
    void toggleActive()  {
        setActive(!m_activated);
    }
    void globalShortcutChanged(QAction *action, const QKeySequence &seq);
    void onWindowAdded(KWin::EffectWindow*);
    void onWindowClosed(KWin::EffectWindow*);
    void onWindowDeleted(KWin::EffectWindow*);

    void appendDesktop();
    void removeDesktop(int d);

    void changeCurrentDesktop(int d);

private slots:
    void onNumberDesktopsChanged(int old);
    void onCurrentDesktopChanged();

private:
    bool isRelevantWithPresentWindows(EffectWindow *w) const;

    void calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm);

    // close animation finished
    void cleanup();

    void updateWindowStates(QMouseEvent* me);

    // borrowed from PresentWindows effect
    void calculateWindowTransformationsNatural(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);
    bool isOverlappingAny(EffectWindow *w, const QHash<EffectWindow*, QRect> &targets, const QRegion &border);
    inline double aspectRatio(EffectWindow *w) {
        return w->width() / double(w->height());
    }
    inline int widthForHeight(EffectWindow *w, int height) {
        return int((height / double(w->height())) * w->width());
    }
    inline int heightForWidth(EffectWindow *w, int width) {
        return int((width / double(w->width())) * w->height());
    }


private:
    // Activation
    bool m_activated {false};
    bool m_hasKeyboardGrab {false};

    // Window data
    QVector<WindowMotionManager> m_motionManagers;
    WindowMotionManager m_thumbMotion;
    EffectWindow* m_highlightWindow {nullptr};

    // Shortcut - needed to toggle the effect
    QList<QKeySequence> shortcut;

    // timeline for toggleActive
    QTimeLine m_toggleTimeline;

    // Desktop currently rendering, could be different thant current desktop
    // when Left/Right key pressed e.g
    // index from 1
    int m_targetDesktop {0};

    QMargins m_desktopMargins;

    DesktopThumbnailManager* m_thumbManager {nullptr};

    QAction *m_showAction;

    QMargins desktopMargins();
};


#endif /* ifndef _DEEPIN_MULTITASKING_H */

