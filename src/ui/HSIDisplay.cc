/*=====================================================================

PIXHAWK Micro Air Vehicle Flying Robotics Toolkit

(c) 2009, 2010 PIXHAWK PROJECT  <http://pixhawk.ethz.ch>

This file is part of the PIXHAWK project

    PIXHAWK is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PIXHAWK is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PIXHAWK. If not, see <http://www.gnu.org/licenses/>.

======================================================================*/

/**
 * @file
 *   @brief Implementation of Horizontal Situation Indicator class
 *
 *   @author Lorenz Meier <mavteam@student.ethz.ch>
 *
 */

#include <QFile>
#include <QStringList>
#include <QPainter>
#include "UASManager.h"
#include "HSIDisplay.h"
#include "MG.h"
#include "QGC.h"

#include <QDebug>

HSIDisplay::HSIDisplay(QWidget *parent) :
        HDDisplay(NULL, parent),
        gpsSatellites(),
        satellitesUsed(0),
        attXSet(0),
        attYSet(0),
        attYawSet(0),
        altitudeSet(1.0),
        posXSet(0),
        posYSet(0),
        posZSet(0),
        attXSaturation(0.5f),
        attYSaturation(0.5f),
        attYawSaturation(0.5f),
        posXSaturation(0.05),
        posYSaturation(0.05),
        altitudeSaturation(1.0),
        lat(0),
        lon(0),
        alt(0),
        globalAvailable(0),
        x(0),
        y(0),
        z(0),
        vx(0),
        vy(0),
        vz(0),
        speed(0),
        localAvailable(0),
        roll(0),
        pitch(0),
        yaw(0.0f),
        bodyXSetCoordinate(0.0f),
        bodyYSetCoordinate(0.0f),
        bodyZSetCoordinate(0.0f),
        bodyYawSet(0.0f),
        uiXSetCoordinate(0.0f),
        uiYSetCoordinate(0.0f),
        uiZSetCoordinate(0.0f),
        uiYawSet(0.0f),
        metricWidth(2.0f),
        positionLock(false),
        attControlEnabled(false),
        xyControlEnabled(false),
        zControlEnabled(false)
{
    connect(UASManager::instance(), SIGNAL(activeUASSet(UASInterface*)), this, SLOT(setActiveUAS(UASInterface*)));
    refreshTimer->setInterval(60);

    // FIXME
    float bottomMargin = 3.0f;
    xCenterPos = vwidth/2.0f;
    yCenterPos = vheight/2.0f - bottomMargin;
}

void HSIDisplay::paintEvent(QPaintEvent * event)
{
    Q_UNUSED(event);
    //paintGL();
    static quint64 interval = 0;
    //qDebug() << "INTERVAL:" << MG::TIME::getGroundTimeNow() - interval << __FILE__ << __LINE__;
    interval = MG::TIME::getGroundTimeNow();
    paintDisplay();
}

void HSIDisplay::paintDisplay()
{
    // Center location of the HSI gauge items

    float bottomMargin = 3.0f;

    // Size of the ring instrument
    const float margin = 0.1f;  // 10% margin of total width on each side
    float baseRadius = (vheight - vheight * 2.0f * margin) / 2.0f - bottomMargin / 2.0f;

    // Draw instruments
    // TESTING THIS SHOULD BE MOVED INTO A QGRAPHICSVIEW
    // Update scaling factor
    // adjust scaling to fit both horizontally and vertically
    scalingFactor = this->width()/vwidth;
    double scalingFactorH = this->height()/vheight;
    if (scalingFactorH < scalingFactor) scalingFactor = scalingFactorH;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

    // Draw background
    painter.fillRect(QRect(0, 0, width(), height()), backgroundColor);

    // Draw status indicators
    QColor statusColor(255, 255, 255);
    QString lockStatus;
    QString xyContrStatus;
    QString zContrStatus;
    QString attContrStatus;

    QColor lockStatusColor;

    if (positionLock)
    {
        lockStatus = tr("LOCK");
        lockStatusColor = QColor(20, 255, 20);
    }
    else
    {
        lockStatus = tr("NO");
        lockStatusColor = QColor(255, 20, 20);
    }

    paintText(tr("POS"), QGC::ColorCyan, 1.8f, 2.0f, 2.5f, &painter);
    painter.setBrush(lockStatusColor);
    painter.setPen(Qt::NoPen);
    painter.drawRect(QRect(refToScreenX(9.5f), refToScreenY(2.0f), refToScreenX(7.0f), refToScreenY(4.0f)));
    paintText(lockStatus, statusColor, 2.8f, 10.0f, 2.0f, &painter);

    // Draw base instrument
    // ----------------------
    painter.setBrush(Qt::NoBrush);
    const QColor ringColor = QColor(200, 250, 200);
    QPen pen;
    pen.setColor(ringColor);
    pen.setWidth(refLineWidthToPen(0.1f));
    painter.setPen(pen);
    const int ringCount = 2;
    for (int i = 0; i < ringCount; i++)
    {
        float radius = (vwidth - vwidth * 2.0f * margin) / (2.0f * i+1) / 2.0f - bottomMargin / 2.0f;
        drawCircle(xCenterPos, yCenterPos, radius, 0.1f, ringColor, &painter);
    }

    // Draw center indicator
    QPolygonF p(3);
    p.replace(0, QPointF(xCenterPos, yCenterPos-2.8484f));
    p.replace(1, QPointF(xCenterPos-2.0f, yCenterPos+2.0f));
    p.replace(2, QPointF(xCenterPos+2.0f, yCenterPos+2.0f));
    drawPolygon(p, &painter);

    // ----------------------

    // Draw satellites
    drawGPS(painter);

    // Draw state indicator

    // Draw position
    QColor positionColor(20, 20, 200);
    drawPositionDirection(xCenterPos, yCenterPos, baseRadius, positionColor, &painter);

    // Draw attitude
    QColor attitudeColor(200, 20, 20);
    drawAttitudeDirection(xCenterPos, yCenterPos, baseRadius, attitudeColor, &painter);


    // Draw position setpoints in body coordinates

    if (uiXSetCoordinate != 0 || uiYSetCoordinate != 0)
    {
        QColor spColor(150, 150, 150);
        drawSetpointXY(uiXSetCoordinate, uiYSetCoordinate, uiYawSet, spColor, painter);
    }

    if (bodyXSetCoordinate != 0 || bodyYSetCoordinate != 0)
    {
        // Draw setpoint
        drawSetpointXY(bodyXSetCoordinate, bodyYSetCoordinate, bodyYawSet, QGC::ColorCyan, painter);
        // Draw travel direction line
        QPointF m(bodyXSetCoordinate, bodyYSetCoordinate);
        // Transform from body to world coordinates
        m = metricWorldToBody(m);
        // Scale from metric body to screen reference units
        QPointF s = metricBodyToRefX(m);
        drawLine(s.x(), s.y(), xCenterPos, yCenterPos, 1.5f, QGC::ColorCyan, &painter);
    }

    // Labels on outer part and bottom

    //if (localAvailable > 0)
    {
        // Position
        QString str;
        str.sprintf("%05.2f %05.2f %05.2f m", x, y, z);
        paintText(str, ringColor, 3.0f, xCenterPos + baseRadius - 30.75f, vheight - 5.0f, &painter);

        // Speed
        str.sprintf("%05.2f m/s", speed);
        paintText(str, ringColor, 3.0f, 10.0f, vheight - 5.0f, &painter);
    }

    // Draw orientation labels
    // Translate and rotate coordinate frame
    painter.translate((xCenterPos)*scalingFactor, (yCenterPos)*scalingFactor);
    painter.rotate((-yaw/(M_PI))*180.0f);
    paintText(tr("N"), ringColor, 3.5f, - 1.0f, - baseRadius - 5.5f, &painter);
    paintText(tr("S"), ringColor, 3.5f, - 1.0f, + baseRadius + 1.5f, &painter);
    paintText(tr("E"), ringColor, 3.5f, + baseRadius + 2.0f, - 1.75f, &painter);
    paintText(tr("W"), ringColor, 3.5f, - baseRadius - 5.5f, - 1.75f, &painter);

//    //  Just for testing
//    bodyXSetCoordinate = 0.95 * bodyXSetCoordinate + 0.05 * uiXSetCoordinate;
//    bodyYSetCoordinate = 0.95 * bodyYSetCoordinate + 0.05 * uiYSetCoordinate;
//    bodyZSetCoordinate = 0.95 * bodyZSetCoordinate + 0.05 * uiZSetCoordinate;
//    bodyYawSet = 0.95 * bodyYawSet + 0.05 * uiYawSet;
}

void HSIDisplay::updatePositionLock(UASInterface* uas, bool lock)
{
    Q_UNUSED(uas);
    positionLock = lock;
}

void HSIDisplay::updateAttitudeControllerEnabled(UASInterface* uas, bool enabled)
{
    Q_UNUSED(uas);
    attControlEnabled = enabled;
}

void HSIDisplay::updatePositionXYControllerEnabled(UASInterface* uas, bool enabled)
{
    Q_UNUSED(uas);
    xyControlEnabled = enabled;
}

void HSIDisplay::updatePositionZControllerEnabled(UASInterface* uas, bool enabled)
{
    Q_UNUSED(uas);
    zControlEnabled = enabled;
}

QPointF HSIDisplay::metricWorldToBody(QPointF world)
{
    // First translate to body-centered coordinates
    // Rotate around -yaw
    QPointF result(cos(yaw) * (world.x() - x) + -sin(yaw) * (world.x() - x), sin(yaw) * (world.y() - y) + cos(yaw) * (world.y() - y));
    return result;
}

QPointF HSIDisplay::metricBodyToWorld(QPointF body)
{
    // First rotate into world coordinates
    // then translate to world position
    QPointF result((cos(yaw) * body.x()) + (sin(yaw) * body.x()) + x, (-sin(yaw) * body.y()) + (cos(yaw) * body.y()) + y);
    return result;
}

QPointF HSIDisplay::screenToMetricBody(QPointF ref)
{
    return QPointF(-((screenToRefY(ref.y()) - yCenterPos)/ vwidth) * metricWidth - x, ((screenToRefX(ref.x()) - xCenterPos) / vwidth) * metricWidth - y);
}

QPointF HSIDisplay::refToMetricBody(QPointF &ref)
{
    return QPointF(-((ref.y() - yCenterPos)/ vwidth) * metricWidth - x, ((ref.x() - xCenterPos) / vwidth) * metricWidth - y);
}

/**
 * @see refToScreenX()
 */
QPointF HSIDisplay::metricBodyToRefX(QPointF &metric)
{
    return QPointF(((metric.y())/ metricWidth) * vwidth + xCenterPos, ((-metric.x()) / metricWidth) * vwidth + yCenterPos);
}

void HSIDisplay::mouseDoubleClickEvent(QMouseEvent * event)
{
    static bool dragStarted;
    static float startX;

    if (event->MouseButtonDblClick)
    {
        //setBodySetpointCoordinateXY(-refToMetric(screenToRefY(event->y()) - yCenterPos), refToMetric(screenToRefX(event->x()) - xCenterPos));

        QPointF p = screenToMetricBody(event->posF());
        setBodySetpointCoordinateXY(p.x(), p.y());
        qDebug() << "Double click at x: " << screenToRefX(event->x()) - xCenterPos << "y:" << screenToRefY(event->y()) - yCenterPos;
    }
    else if (event->MouseButtonPress)
    {
        startX = event->globalX();
        if (event->button() == Qt::RightButton)
        {
            // Start tracking mouse move
            dragStarted = true;
        }
        else if (event->button() == Qt::LeftButton)
        {

        }
    }
    else if (event->MouseButtonRelease)
    {
        dragStarted = false;
    }
    else if (event->MouseMove)
    {
        if (dragStarted) uiYawSet += (startX - event->globalX()) / this->frameSize().width();
    }
}

/**
 *
 * @param uas the UAS/MAV to monitor/display with the HUD
 */
void HSIDisplay::setActiveUAS(UASInterface* uas)
{
    if (this->uas != NULL && this->uas != uas)
    {
        // Disconnect any previously connected active MAV
        //disconnect(uas, SIGNAL(valueChanged(UASInterface*,QString,double,quint64)), this, SLOT(updateValue(UASInterface*,QString,double,quint64)));
    }


    HDDisplay::setActiveUAS(uas);
    //qDebug() << "ATTEMPTING TO SET UAS";


    connect(uas, SIGNAL(gpsSatelliteStatusChanged(int,int,float,float,float,bool)), this, SLOT(updateSatellite(int,int,float,float,float,bool)));
    connect(uas, SIGNAL(localPositionChanged(UASInterface*,double,double,double,quint64)), this, SLOT(updateLocalPosition(UASInterface*,double,double,double,quint64)));
    connect(uas, SIGNAL(globalPositionChanged(UASInterface*,double,double,double,quint64)), this, SLOT(updateGlobalPosition(UASInterface*,double,double,double,quint64)));
    connect(uas, SIGNAL(attitudeThrustSetPointChanged(UASInterface*,double,double,double,double,quint64)), this, SLOT(updateAttitudeSetpoints(UASInterface*,double,double,double,double,quint64)));
    connect(uas, SIGNAL(positionSetPointsChanged(int,float,float,float,float,quint64)), this, SLOT(updatePositionSetpoints(int,float,float,float,float,quint64)));
    connect(uas, SIGNAL(speedChanged(UASInterface*,double,double,double,quint64)), this, SLOT(updateSpeed(UASInterface*,double,double,double,quint64)));
    connect(uas, SIGNAL(attitudeChanged(UASInterface*,double,double,double,quint64)), this, SLOT(updateAttitude(UASInterface*,double,double,double,quint64)));

    // Now connect the new UAS

    //if (this->uas != uas)
    // {
    //qDebug() << "UAS SET!" << "ID:" << uas->getUASID();
    // Setup communication
    //connect(uas, SIGNAL(valueChanged(UASInterface*,QString,double,quint64)), this, SLOT(updateValue(UASInterface*,QString,double,quint64)));
    //}
}

void HSIDisplay::updateSpeed(UASInterface* uas, double vx, double vy, double vz, quint64 time)
{
    Q_UNUSED(uas);
    Q_UNUSED(time);
    this->vx = vx;
    this->vy = vy;
    this->vz = vz;
    this->speed = sqrt(pow(vx, 2.0f) + pow(vy, 2.0f) + pow(vz, 2.0f));
}

void HSIDisplay::setBodySetpointCoordinateXY(double x, double y)
{
    // Set coordinates and send them out to MAV

    QPointF sp(x, y);
    sp = metricBodyToWorld(sp);
    uiXSetCoordinate = sp.x();
    uiYSetCoordinate = sp.y();

    if (uas)
    {
        uas->setLocalPositionSetpoint(uiXSetCoordinate, uiYSetCoordinate, uiZSetCoordinate, uiYawSet);
        qDebug() << "Setting new setpoint at x: " << x << "metric y:" << y;
    }
}

void HSIDisplay::setBodySetpointCoordinateZ(double z)
{
    // Set coordinates and send them out to MAV
    uiZSetCoordinate = z;
}

void HSIDisplay::sendBodySetPointCoordinates()
{
    // Send the coordinates to the MAV
}

void HSIDisplay::updateAttitudeSetpoints(UASInterface* uas, double rollDesired, double pitchDesired, double yawDesired, double thrustDesired, quint64 usec)
{
    Q_UNUSED(uas);
    Q_UNUSED(usec);
    attXSet = pitchDesired;
    attYSet = rollDesired;
    attYawSet = yawDesired;
    altitudeSet = thrustDesired;
}

void HSIDisplay::updateAttitude(UASInterface* uas, double roll, double pitch, double yaw, quint64 time)
{
    Q_UNUSED(uas);
    Q_UNUSED(time);
    this->roll = roll;
    this->pitch = pitch;
    this->yaw = yaw;
}

void HSIDisplay::updatePositionSetpoints(int uasid, float xDesired, float yDesired, float zDesired, float yawDesired, quint64 usec)
{
    Q_UNUSED(usec);
    Q_UNUSED(uasid);
    bodyXSetCoordinate = xDesired;
    bodyYSetCoordinate = yDesired;
    bodyZSetCoordinate = zDesired;
    bodyYawSet = yawDesired;
//    posXSet = xDesired;
//    posYSet = yDesired;
//    posZSet = zDesired;
//    posYawSet = yawDesired;
}

void HSIDisplay::updateLocalPosition(UASInterface*, double x, double y, double z, quint64 usec)
{
    this->x = x;
    this->y = y;
    this->z = z;
    localAvailable = usec;
}

void HSIDisplay::updateGlobalPosition(UASInterface*, double lat, double lon, double alt, quint64 usec)
{
    this->lat = lat;
    this->lon = lon;
    this->alt = alt;
    globalAvailable = usec;
}

void HSIDisplay::updateSatellite(int uasid, int satid, float elevation, float azimuth, float snr, bool used)
{
    Q_UNUSED(uasid);
    //qDebug() << "UPDATED SATELLITE";
    // If slot is empty, insert object
    if (gpsSatellites.contains(satid))
    {
        gpsSatellites.value(satid)->update(satid, elevation, azimuth, snr, used);
    }
    else
    {
        gpsSatellites.insert(satid, new GPSSatellite(satid, elevation, azimuth, snr, used));
    }
}

QColor HSIDisplay::getColorForSNR(float snr)
{
    QColor color;
    if (snr > 0 && snr < 30)
    {
        color = QColor(250, 10, 10);
    }
    else if (snr >= 30 && snr < 35)
    {
        color = QColor(230, 230, 10);
    }
    else if (snr >= 35 && snr < 40)
    {
        color = QColor(90, 200, 90);
    }
    else if (snr >= 40)
    {
        color = QColor(20, 200, 20);
    }
    else
    {
        color = QColor(180, 180, 180);
    }
    return color;
}

void HSIDisplay::drawSetpointXY(float x, float y, float yaw, const QColor &color, QPainter &painter)
{
    float radius = vwidth / 20.0f;
    QPen pen(color);
    pen.setWidthF(refLineWidthToPen(0.4f));
    pen.setColor(color);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPointF in(x, y);
    // Transform from body to world coordinates
    in = metricWorldToBody(in);
    // Scale from metric to screen reference coordinates
    QPointF p = metricBodyToRefX(in);
    drawCircle(p.x(), p.y(), radius, 0.4f, color, &painter);
    radius *= 0.8;
    drawLine(p.x(), p.y(), p.x()+sin(yaw) * radius, p.y()-cos(yaw) * radius, refLineWidthToPen(0.4f), color, &painter);
    painter.setBrush(color);
    drawCircle(p.x(), p.y(), radius * 0.1f, 0.1f, color, &painter);
}

void HSIDisplay::drawSafetyArea(const QPointF &topLeft, const QPointF &bottomRight, const QColor &color, QPainter &painter)
{
    QPen pen(color);
    pen.setWidthF(refLineWidthToPen(0.1f));
    pen.setColor(color);
    painter.setPen(pen);
    painter.drawRect(QRectF(topLeft, bottomRight));
}

void HSIDisplay::drawGPS(QPainter &painter)
{
    float xCenter = xCenterPos;
    float yCenter = xCenterPos;
    // Max satellite circle radius

    const float margin = 0.15f;  // 20% margin of total width on each side
    float radius = (vwidth - vwidth * 2.0f * margin) / 2.0f;
    quint64 currTime = MG::TIME::getGroundTimeNowUsecs();

    // Draw satellite labels
    //    QString label;
    //    label.sprintf("%05.1f", value);
    //    paintText(label, color, 4.5f, xRef-7.5f, yRef-2.0f, painter);

    QMapIterator<int, GPSSatellite*> i(gpsSatellites);
    while (i.hasNext())
    {
        i.next();
        GPSSatellite* sat = i.value();

        // Check if update is not older than 5 seconds, else delete satellite
        if (sat->lastUpdate + 1000000 < currTime)
        {
            // Delete and go to next satellite
            gpsSatellites.remove(i.key());
            if (i.hasNext())
            {
                i.next();
                sat = i.value();
            }
            else
            {
                continue;
            }
        }

        if (sat)
        {
            // Draw satellite
            QBrush brush;
            QColor color = getColorForSNR(sat->snr);
            brush.setColor(color);
            if (sat->used)
            {
                brush.setStyle(Qt::SolidPattern);
            }
            else
            {
                brush.setStyle(Qt::NoBrush);
            }
            painter.setPen(Qt::SolidLine);
            painter.setPen(color);
            painter.setBrush(brush);

            float xPos = xCenter + (sin(((sat->azimuth/255.0f)*360.0f)/180.0f * M_PI) * cos(sat->elevation/180.0f * M_PI)) * radius;
            float yPos = yCenter - (cos(((sat->azimuth/255.0f)*360.0f)/180.0f * M_PI) * cos(sat->elevation/180.0f * M_PI)) * radius;

            // Draw circle for satellite, filled for used satellites
            drawCircle(xPos, yPos, vwidth*0.02f, 1.0f, color, &painter);
            // Draw satellite PRN
            paintText(QString::number(sat->id), QColor(255, 255, 255), 2.9f, xPos+1.7f, yPos+2.0f, &painter);
        }
    }
}

void HSIDisplay::drawObjects(QPainter &painter)
{
    Q_UNUSED(painter);
}

void HSIDisplay::drawPositionDirection(float xRef, float yRef, float radius, const QColor& color, QPainter* painter)
{
    // Draw the needle
    const float maxWidth = radius / 10.0f;
    const float minWidth = maxWidth * 0.3f;

    float angle = atan2(posXSet, -posYSet);
    angle -= M_PI/2.0f;

    QPolygonF p(6);

    //radius *= ((posXSaturation + posYSaturation) - sqrt(pow(posXSet, 2), pow(posYSet, 2))) / (2*posXSaturation);

    radius *= sqrt(pow(posXSet, 2) + pow(posYSet, 2)) / sqrt(posXSaturation + posYSaturation);

    p.replace(0, QPointF(xRef-maxWidth/2.0f, yRef-radius * 0.4f));
    p.replace(1, QPointF(xRef-minWidth/2.0f, yRef-radius * 0.9f));
    p.replace(2, QPointF(xRef+minWidth/2.0f, yRef-radius * 0.9f));
    p.replace(3, QPointF(xRef+maxWidth/2.0f, yRef-radius * 0.4f));
    p.replace(4, QPointF(xRef,               yRef-radius * 0.36f));
    p.replace(5, QPointF(xRef-maxWidth/2.0f, yRef-radius * 0.4f));

    rotatePolygonClockWiseRad(p, angle, QPointF(xRef, yRef));

    QBrush indexBrush;
    indexBrush.setColor(color);
    indexBrush.setStyle(Qt::SolidPattern);
    painter->setPen(Qt::SolidLine);
    painter->setPen(color);
    painter->setBrush(indexBrush);
    drawPolygon(p, painter);

    //qDebug() << "DRAWING POS SETPOINT X:" << posXSet << "Y:" << posYSet << angle;
}

void HSIDisplay::drawAttitudeDirection(float xRef, float yRef, float radius, const QColor& color, QPainter* painter)
{
    // Draw the needle
    const float maxWidth = radius / 10.0f;
    const float minWidth = maxWidth * 0.3f;

    float angle = atan2(attXSet, attYSet);
    angle -= M_PI/2.0f;

    radius *= sqrt(pow(attXSet, 2) + pow(attYSet, 2)) / sqrt(attXSaturation + attYSaturation);

    QPolygonF p(6);

    p.replace(0, QPointF(xRef-maxWidth/2.0f, yRef-radius * 0.4f));
    p.replace(1, QPointF(xRef-minWidth/2.0f, yRef-radius * 0.9f));
    p.replace(2, QPointF(xRef+minWidth/2.0f, yRef-radius * 0.9f));
    p.replace(3, QPointF(xRef+maxWidth/2.0f, yRef-radius * 0.4f));
    p.replace(4, QPointF(xRef,               yRef-radius * 0.36f));
    p.replace(5, QPointF(xRef-maxWidth/2.0f, yRef-radius * 0.4f));

    rotatePolygonClockWiseRad(p, angle, QPointF(xRef, yRef));

    QBrush indexBrush;
    indexBrush.setColor(color);
    indexBrush.setStyle(Qt::SolidPattern);
    painter->setPen(Qt::SolidLine);
    painter->setPen(color);
    painter->setBrush(indexBrush);
    drawPolygon(p, painter);

    // TODO Draw Yaw indicator

    //qDebug() << "DRAWING ATT SETPOINT X:" << attXSet << "Y:" << attYSet << angle;
}

void HSIDisplay::drawAltitudeSetpoint(float xRef, float yRef, float radius, const QColor& color, QPainter* painter)
{
    // Draw the circle
    QPen circlePen(Qt::SolidLine);
    circlePen.setWidth(refLineWidthToPen(0.5f));
    circlePen.setColor(color);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(circlePen);
    drawCircle(xRef, yRef, radius, 200.0f, color, painter);
    //drawCircle(xRef, yRef, radius, 200.0f, 170.0f, 1.0f, color, painter);

    //    // Draw the value
    //    QString label;
    //    label.sprintf("%05.1f", value);
    //    paintText(label, color, 4.5f, xRef-7.5f, yRef-2.0f, painter);
}

void HSIDisplay::updateJoystick(double roll, double pitch, double yaw, double thrust, int xHat, int yHat)
{

}

void HSIDisplay::pressKey(int key)
{

}