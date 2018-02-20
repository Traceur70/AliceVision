// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "common.hpp"
#include <aliceVision/common/fileIO.hpp>
#include <aliceVision/structures/geometry.hpp>
#include <aliceVision/structures/geometryTriTri.hpp>
#include <aliceVision/structures/Matrix3x3.hpp>
#include <aliceVision/structures/Matrix3x4.hpp>
#include <aliceVision/structures/OrientedPoint.hpp>
#include <aliceVision/structures/Pixel.hpp>
#include <aliceVision/structures/SeedPoint.hpp>

#ifdef _WIN32
#include "Psapi.h"
#endif

namespace aliceVision {

bool get2dLineImageIntersection(Point2d* pFrom, Point2d* pTo, Point2d linePoint1, Point2d linePoint2,
                                const MultiViewParams* mp, int camId)
{
    Point2d v = linePoint2 - linePoint1;

    if(v.size() < FLT_EPSILON)
    {
        return false; // bad configuration ... forward motion with cental ref pixel
    }

    v = v.normalize();

    float a = -v.y;
    float b = v.x;
    float c = -a * linePoint1.x - b * linePoint1.y;

    int intersections = 0;
    float rw = (float)mp->mip->getWidth(camId);
    float rh = (float)mp->mip->getHeight(camId);

    // ax + by + c = 0

    // right epip line intersection with the left side of the right image
    // a*0 + b*y + c = 0; y = -c / b;
    float x = 0;
    float y = -c / b;
    if((y >= 0) && (y < rh))
    {
        *pFrom = Point2d(x, y);
        intersections++;
    }

    // right epip line intersection with the right side of the right image
    // a*rw + b*y + c = 0; y = (-c-a*rw) / b;
    x = rw;
    y = (-c - a * rw) / b;
    if((y >= 0) && (y < rh))
    {
        if(intersections == 0)
        {
            *pFrom = Point2d(x, y);
        }
        else
        {
            *pTo = Point2d(x, y);
        }
        intersections++;
    }

    // right epip line intersection with the top side of the right image
    // a*x + b*0 + c = 0; x = -c / a;
    x = -c / a;
    y = 0;
    if((x >= 0) && (x < rw))
    {
        if(intersections == 0)
        {
            *pFrom = Point2d(x, y);
        }
        else
        {
            *pTo = Point2d(x, y);
        }
        intersections++;
    }

    // right epip line intersection with the bottom side of the right image
    // a*x + b*rh + c = 0; x = (-c-b*rh) / a;
    x = (-c - b * rh) / a;
    y = rh;
    if((x >= 0) && (x < rw))
    {
        if(intersections == 0)
        {
            *pFrom = Point2d(x, y);
        }
        else
        {
            *pTo = Point2d(x, y);
        }
        intersections++;
    }

    if(intersections == 2)
    {
        if((linePoint1 - *pFrom).size() > (linePoint1 - *pTo).size())
        {
            Point2d p = *pFrom;
            *pFrom = *pTo;
            *pTo = p;
        }
        return true;
    }

    return false;
}

bool getTarEpipolarDirectedLine(Point2d* pFromTar, Point2d* pToTar, Point2d refpix, int refCam, int tarCam,
                                const MultiViewParams* mp)
{
    const Matrix3x4& rP = mp->camArr[refCam];
    const Matrix3x4& tP = mp->camArr[tarCam];

    Point3d rC;
    Matrix3x3 rR;
    Matrix3x3 riR;
    Matrix3x3 rK;
    Matrix3x3 riK;
    Matrix3x3 riP;
    mp->decomposeProjectionMatrix(rC, rR, riR, rK, riK, riP, rP);

    Point3d tC;
    Matrix3x3 tR;
    Matrix3x3 tiR;
    Matrix3x3 tK;
    Matrix3x3 tiK;
    Matrix3x3 tiP;
    mp->decomposeProjectionMatrix(tC, tR, tiR, tK, tiK, tiP, tP);

    Point3d refvect = riP * refpix;
    refvect = refvect.normalize();

    float d = (rC - tC).size();
    Point3d X = refvect * d + rC;
    Point2d tarpix1;
    mp->getPixelFor3DPoint(&tarpix1, X, tP);

    X = refvect * d * 500.0 + rC;
    Point2d tarpix2;
    mp->getPixelFor3DPoint(&tarpix2, X, tP);

    return get2dLineImageIntersection(pFromTar, pToTar, tarpix1, tarpix2, mp, tarCam);
}

bool triangulateMatch(Point3d& out, const Point2d& refpix, const Point2d& tarpix, int refCam, int tarCam,
                      const MultiViewParams* mp)
{
    Point3d refvect = mp->iCamArr[refCam] * refpix;
    refvect = refvect.normalize();
    Point3d refpoint = refvect + mp->CArr[refCam];

    Point3d tarvect = mp->iCamArr[tarCam] * tarpix;
    tarvect = tarvect.normalize();
    Point3d tarpoint = tarvect + mp->CArr[tarCam];

    float k, l;
    Point3d lli1, lli2;

    return lineLineIntersect(&k, &l, &out, &lli1, &lli2, mp->CArr[refCam], refpoint, mp->CArr[tarCam], tarpoint);
}

long initEstimate()
{
    return clock();
}

void printfEstimate(int i, int n, long startTime)
{
    if((int)((float)i / ((float)n / 100.0)) != (int)((float)(i + 1) / ((float)n / 100.0)))
    {
        int perc = (int)((float)i / ((float)n / 100.0));

        long t2 = clock();

        long mils = (long)(((float)(t2 - startTime) / (float)perc) * 100.0) - (t2 - startTime);
        long seco = mils / CLOCKS_PER_SEC;
        long minu = seco / 60;
        long hour = minu / 60;
        long days = std::abs(hour / 24);

        int ihour = std::abs(hour % 60);
        int iminu = std::abs(minu % 60);
        int iseco = std::abs(seco % 60);

        float d1 = (float)(t2 - startTime) / (float)CLOCKS_PER_SEC;
        int elapsedsec = (int)d1 - (int)floor(d1 / 60.0) * 60;

        if(elapsedsec > 15)
        {
            printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%03i%% - remaining "
                   "time %02li days %02i:%02i:%02i",
                   perc, days, ihour, iminu, iseco);
        }
    }
}

void finishEstimate()
{
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    printf("                                      ");
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
}

std::string printfElapsedTime(long t1, std::string prefix)
{
    long t2 = clock();
    float d1 = (float)(t2 - t1) / (float)CLOCKS_PER_SEC;

    int min = (int)floor(d1 / 60.0);
    int sec = (int)d1 - (int)floor(d1 / 60.0) * 60;
    int mil = (int)((d1 - (int)floor(d1)) * 1000);

    printf("%s elapsed time %i minutes %i seconds %i miliseconds\n", prefix.c_str(), min, sec, mil);
    std::string out = prefix + " elapsed time " + num2strTwoDecimal(min) + " minutes " + num2strTwoDecimal(sec) +
                      " seconds " + num2strThreeDigits(mil) + " miliseconds\n";
    return out;
}

bool checkPair(const Point3d& p, int rc, int tc, const MultiViewParams* mp, float minAng, float maxAng)
{
    float ps1 = mp->getCamPixelSize(p, rc);
    float ps2 = mp->getCamPixelSize(p, tc);
    float ang = angleBetwABandAC(p, mp->CArr[rc], mp->CArr[tc]);

    return ((std::min(ps1, ps2) > std::max(ps1, ps2) * 0.8) && (ang >= minAng) && (ang <= maxAng));
}

bool checkCamPairAngle(int rc, int tc, const MultiViewParams* mp, float minAng, float maxAng)
{
    if(rc == tc)
    {
        return false;
    }

    Point3d rn = mp->iRArr[rc] * Point3d(0.0, 0.0, 1.0);
    Point3d tn = mp->iRArr[tc] * Point3d(0.0, 0.0, 1.0);
    float a = angleBetwV1andV2(rn, tn);

    return ((a >= minAng) && (a <= maxAng));
}

// hexahedron format ... 0-3 frontal face, 4-7 back face
void getHexahedronTriangles(Point3d tris[12][3], Point3d hexah[8])
{
    Point3d a1 = hexah[0];
    Point3d a2 = hexah[4];
    Point3d b1 = hexah[1];
    Point3d b2 = hexah[5];
    Point3d c1 = hexah[2];
    Point3d c2 = hexah[6];
    Point3d d1 = hexah[3];
    Point3d d2 = hexah[7];

    tris[0][0] = a1;
    tris[0][1] = a2;
    tris[0][2] = b1;
    tris[1][0] = b1;
    tris[1][1] = a2;
    tris[1][2] = b2;

    tris[2][0] = b1;
    tris[2][1] = b2;
    tris[2][2] = c2;
    tris[3][0] = b1;
    tris[3][1] = c2;
    tris[3][2] = c1;

    tris[4][0] = c1;
    tris[4][1] = c2;
    tris[4][2] = d2;
    tris[5][0] = c1;
    tris[5][1] = d2;
    tris[5][2] = d1;

    tris[6][0] = d1;
    tris[6][1] = d2;
    tris[6][2] = a2;
    tris[7][0] = d1;
    tris[7][1] = a2;
    tris[7][2] = a1;

    tris[8][0] = a1;
    tris[8][1] = b1;
    tris[8][2] = d1;
    tris[9][0] = b1;
    tris[9][1] = c1;
    tris[9][2] = d1;

    tris[10][0] = d2;
    tris[10][1] = b2;
    tris[10][2] = a2;
    tris[11][0] = d2;
    tris[11][1] = c2;
    tris[11][2] = b2;
}

// hexahedron format ... 0-3 frontal face, 4-7 back face
void getCamHexahedron(const MultiViewParams* mp, Point3d hexah[8], int cam, float mind, float maxd)
{
    float w = (float)mp->mip->getWidth(cam);
    float h = (float)mp->mip->getHeight(cam);
    hexah[0] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(0.0f, 0.0f)).normalize() * mind;
    hexah[4] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(0.0f, 0.0f)).normalize() * maxd;
    hexah[1] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(w, 0.0f)).normalize() * mind;
    hexah[5] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(w, 0.0f)).normalize() * maxd;
    hexah[2] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(w, h)).normalize() * mind;
    hexah[6] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(w, h)).normalize() * maxd;
    hexah[3] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(0.0f, h)).normalize() * mind;
    hexah[7] = mp->CArr[cam] + (mp->iCamArr[cam] * Point2d(0.0f, h)).normalize() * maxd;
}

// hexahedron format ... 0-3 frontal face, 4-7 back face
bool intersectsHexahedronHexahedron(Point3d rchex[8], Point3d tchex[8])
{
    Point3d rctris[12][3];
    Point3d tctris[12][3];
    getHexahedronTriangles(rctris, rchex);
    getHexahedronTriangles(tctris, tchex);

    for(int t1 = 0; t1 < 12; t1++)
    {
        for(int t2 = 0; t2 < 12; t2++)
        {
            if(interectsTriangleTriangle(rctris[t1], tctris[t2]))
            {
                return true;
            }
        }
    }

    for(int i = 0; i < 8; i++)
    {
        if(isPointInHexahedron(rchex[i], tchex))
        {
            return true;
        }
        if(isPointInHexahedron(tchex[i], rchex))
        {
            return true;
        }
    }

    return false;
}

StaticVector<Point3d>* triangleHexahedronIntersection(Point3d& A, Point3d& B, Point3d& C, Point3d hexah[8])
{
    Point3d tris[12][3];
    getHexahedronTriangles(tris, hexah);

    StaticVector<Point3d>* out = new StaticVector<Point3d>(40);
    for(int i = 0; i < 12; i++)
    {
        Point3d a = tris[i][0];
        Point3d b = tris[i][1];
        Point3d c = tris[i][2];

        int coplanar;
        Point3d i1, i2;

        bool ok = (bool)tri_tri_intersect_with_isectline(A.m, B.m, C.m, a.m, b.m, c.m, &coplanar, i1.m, i2.m);
        if(ok)
        {
            out->push_back(i1);
            out->push_back(i2);
        }
    }

    return out;
}

StaticVector<Point3d>* lineSegmentHexahedronIntersection(Point3d& linePoint1, Point3d& linePoint2, Point3d hexah[8])
{
    Point3d tris[12][3];
    getHexahedronTriangles(tris, hexah);

    StaticVector<Point3d>* out = new StaticVector<Point3d>(40);
    for(int i = 0; i < 12; i++)
    {
        Point3d a = tris[i][0];
        Point3d b = tris[i][1];
        Point3d c = tris[i][2];
        Point3d lpi;

        if(isLineSegmentInTriangle(lpi, a, b, c, linePoint1, linePoint2))
        {
            out->push_back(lpi);
        }
    }

    return out;
}

StaticVector<Point3d>* triangleRectangleIntersection(Point3d& A, Point3d& B, Point3d& C, const MultiViewParams* mp, int rc,
                                                     Point2d P[4])
{
    float maxd =
        std::max(std::max((mp->CArr[rc] - A).size(), (mp->CArr[rc] - B).size()), (mp->CArr[rc] - C).size()) * 1000.0f;

    StaticVector<Point3d>* out = new StaticVector<Point3d>(40);

    Point3d a, b, c;
    int coplanar;
    Point3d i1, i2;

    a = mp->CArr[rc];
    b = mp->CArr[rc] + (mp->iCamArr[rc] * P[0]).normalize() * maxd;
    c = mp->CArr[rc] + (mp->iCamArr[rc] * P[1]).normalize() * maxd;
    bool ok = (bool)tri_tri_intersect_with_isectline(A.m, B.m, C.m, a.m, b.m, c.m, &coplanar, i1.m, i2.m);
    if(ok)
    {
        out->push_back(i1);
        out->push_back(i2);
    }

    a = mp->CArr[rc];
    b = mp->CArr[rc] + (mp->iCamArr[rc] * P[1]).normalize() * maxd;
    c = mp->CArr[rc] + (mp->iCamArr[rc] * P[2]).normalize() * maxd;
    ok = (bool)tri_tri_intersect_with_isectline(A.m, B.m, C.m, a.m, b.m, c.m, &coplanar, i1.m, i2.m);
    if(ok)
    {
        out->push_back(i1);
        out->push_back(i2);
    }

    a = mp->CArr[rc];
    b = mp->CArr[rc] + (mp->iCamArr[rc] * P[2]).normalize() * maxd;
    c = mp->CArr[rc] + (mp->iCamArr[rc] * P[3]).normalize() * maxd;
    ok = (bool)tri_tri_intersect_with_isectline(A.m, B.m, C.m, a.m, b.m, c.m, &coplanar, i1.m, i2.m);
    if(ok)
    {
        out->push_back(i1);
        out->push_back(i2);
    }

    a = mp->CArr[rc];
    b = mp->CArr[rc] + (mp->iCamArr[rc] * P[3]).normalize() * maxd;
    c = mp->CArr[rc] + (mp->iCamArr[rc] * P[0]).normalize() * maxd;
    ok = (bool)tri_tri_intersect_with_isectline(A.m, B.m, C.m, a.m, b.m, c.m, &coplanar, i1.m, i2.m);
    if(ok)
    {
        out->push_back(i1);
        out->push_back(i2);
    }

    // Point3d lp;
    // if lineSegmentPlaneIntersect(&lp,A,B,mp->CArr[rc],n);

    return out;
}

bool isPointInHexahedron(const Point3d& p, const Point3d* hexah)
{
    Point3d a = hexah[0];
    Point3d b = hexah[1];
    Point3d c = hexah[3];
    Point3d d = hexah[4];
    Point3d n = cross(a - b, b - c).normalize();
    float d1 = orientedPointPlaneDistance(p, a, n);
    float d2 = orientedPointPlaneDistance(d, a, n);
    if(d1 * d2 < 0.0)
        return false;

    a = hexah[0];
    b = hexah[1];
    c = hexah[4];
    d = hexah[3];
    n = cross(a - b, b - c).normalize();
    d1 = orientedPointPlaneDistance(p, a, n);
    d2 = orientedPointPlaneDistance(d, a, n);
    if(d1 * d2 < 0.0)
        return false;

    a = hexah[1];
    b = hexah[2];
    c = hexah[5];
    d = hexah[0];
    n = cross(a - b, b - c).normalize();
    d1 = orientedPointPlaneDistance(p, a, n);
    d2 = orientedPointPlaneDistance(d, a, n);
    if(d1 * d2 < 0.0)
        return false;

    a = hexah[2];
    b = hexah[6];
    c = hexah[3];
    d = hexah[1];
    n = cross(a - b, b - c).normalize();
    d1 = orientedPointPlaneDistance(p, a, n);
    d2 = orientedPointPlaneDistance(d, a, n);
    if(d1 * d2 < 0.0)
        return false;

    a = hexah[0];
    b = hexah[4];
    c = hexah[3];
    d = hexah[1];
    n = cross(a - b, b - c).normalize();
    d1 = orientedPointPlaneDistance(p, a, n);
    d2 = orientedPointPlaneDistance(d, a, n);
    if(d1 * d2 < 0.0)
        return false;

    a = hexah[4];
    b = hexah[5];
    c = hexah[7];
    d = hexah[0];
    n = cross(a - b, b - c).normalize();
    d1 = orientedPointPlaneDistance(p, a, n);
    d2 = orientedPointPlaneDistance(d, a, n);
    return d1 * d2 >= 0.0;
}

void inflateHexahedron(Point3d hexahIn[8], Point3d hexahOut[8], float scale)
{
    Point3d cg = Point3d(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < 8; i++)
    {
        cg = cg + hexahIn[i];
    }
    cg = cg / 8.0f;

    for(int i = 0; i < 8; i++)
    {
        hexahOut[i] = cg + (hexahIn[i] - cg) * scale;
    }
}

/* //matlab code to simulate kernel voting of array of similarities

close all
clear all
clc

a=1.0;
c=0.1;
x=-1.0:0.01:1.0;

sims = [-0.8 -0.83 -0.82 -0.6 -0.4 -0.2];
%sims = [-0.8 -0.6 -0.4 -0.2];

y = zeros(size(x,2));
for i=1:size(x,2)
        y(i) = sum(a*exp(-(x(i)-sims).^2/(2*c^2)));
end


figure
hold on

for i=1:size(sims,2)
        plot(x,a*exp(-(x-sims(i)).^2/(2*c^2)));
    plot([sims(i) sims(i)],[max(y(:)) 0],'r-');
end


plot(x,y)

*/

StaticVector<StaticVector<int>*>* convertObjectsCamsToCamsObjects(const MultiViewParams* mp,
                                                                  StaticVector<StaticVector<int>*>* ptsCams)
{
    StaticVector<int>* nCamsPts = new StaticVector<int>(mp->ncams);
    nCamsPts->resize_with(mp->ncams, 0);
    for(int i = 0; i < ptsCams->size(); i++)
    {
        for(int j = 0; j < sizeOfStaticVector<int>((*ptsCams)[i]); j++)
        {
            int rc = (*(*ptsCams)[i])[j];
            if((rc >= 0) && (rc < mp->ncams))
            {
                (*nCamsPts)[rc]++;
            }
            else
            {
                printf("WARNING convertObjectsCamsToCamsObjects %i \n", rc);
            }
        }
    }

    StaticVector<StaticVector<int>*>* camsPts = new StaticVector<StaticVector<int>*>(mp->ncams);
    for(int rc = 0; rc < mp->ncams; rc++)
    {
        camsPts->push_back(new StaticVector<int>((*nCamsPts)[rc]));
    }

    for(int i = 0; i < ptsCams->size(); i++)
    {
        for(int j = 0; j < sizeOfStaticVector<int>((*ptsCams)[i]); j++)
        {
            int rc = (*(*ptsCams)[i])[j];
            if((rc >= 0) && (rc < mp->ncams))
            {
                (*camsPts)[rc]->push_back(i);
            }
        }
    }

    return camsPts;
}

StaticVector<StaticVector<Pixel>*>* convertObjectsCamsToCamsObjects(const MultiViewParams* mp,
                                                                    StaticVector<StaticVector<Pixel>*>* ptsCams)
{
    StaticVector<int>* nCamsPts = new StaticVector<int>(mp->ncams);
    nCamsPts->resize_with(mp->ncams, 0);
    for(int i = 0; i < ptsCams->size(); i++)
    {
        for(int j = 0; j < sizeOfStaticVector<Pixel>((*ptsCams)[i]); j++)
        {
            int rc = (*(*ptsCams)[i])[j].x;
            (*nCamsPts)[rc]++;
        }
    }

    StaticVector<StaticVector<Pixel>*>* camsPts = new StaticVector<StaticVector<Pixel>*>(mp->ncams);
    for(int rc = 0; rc < mp->ncams; rc++)
    {
        camsPts->push_back(new StaticVector<Pixel>((*nCamsPts)[rc]));
    }

    for(int i = 0; i < ptsCams->size(); i++)
    {
        for(int j = 0; j < sizeOfStaticVector<Pixel>((*ptsCams)[i]); j++)
        {
            int rc = (*(*ptsCams)[i])[j].x;
            int value = (*(*ptsCams)[i])[j].y;
            (*camsPts)[rc]->push_back(Pixel(i, value));
        }
    }

    return camsPts;
}

int computeStep(MultiViewInputParams* mip, int scale, int maxWidth, int maxHeight)
{
    int step = 1;
    int ow = mip->getMaxImageWidth() / scale;
    int oh = mip->getMaxImageHeight() / scale;
    int g_Width = mip->getMaxImageWidth() / scale;
    int g_Height = mip->getMaxImageHeight() / scale;
    while((g_Width > maxWidth) || (g_Height > maxHeight))
    {
        step++;
        g_Width = ow / step;
        g_Height = oh / step;
    }
    return step;
}

StaticVector<Point3d>* computeVoxels(const Point3d* space, const Voxel& dimensions)
{
    float voxelDimX = (float)dimensions.x;
    float voxelDimY = (float)dimensions.y;
    float voxelDimZ = (float)dimensions.z;

    Point3d ox = space[0];
    Point3d vx = (space[1] - space[0]).normalize();
    Point3d vy = (space[3] - space[0]).normalize();
    Point3d vz = (space[4] - space[0]).normalize();
    float sx = (space[1] - space[0]).size();
    float sy = (space[3] - space[0]).size();
    float sz = (space[4] - space[0]).size();
    float stepx = sx / voxelDimX;
    float stepy = sy / voxelDimY;
    float stepz = sz / voxelDimZ;

    int nvoxels = dimensions.x * dimensions.y * dimensions.z;
    StaticVector<Point3d>* voxels = new StaticVector<Point3d>(nvoxels * 8);
    voxels->resize(nvoxels * 8);

    // printf("%i %i %i %i\n",dimensions.x,dimensions.y,dimensions.z,nvoxels,voxels->size());

    int id = 0;
    for(int xp = 0; xp < dimensions.x; xp++)
    {
        for(int yp = 0; yp < dimensions.y; yp++)
        {
            for(int zp = 0; zp < dimensions.z; zp++)
            {
                float x = (float)xp * stepx;
                float y = (float)yp * stepy;
                float z = (float)zp * stepz;
                (*voxels)[id * 8 + 0] = ox + vx * x + vy * y + vz * z;                      // x,   y,   z
                (*voxels)[id * 8 + 1] = ox + vx * (x + stepx) + vy * y + vz * z;            // x+1, y,   z
                (*voxels)[id * 8 + 2] = ox + vx * (x + stepx) + vy * (y + stepy) + vz * z;  // x+1, y+1, z
                (*voxels)[id * 8 + 3] = ox + vx * x + vy * (y + stepy) + vz * z;            // x,   y+1, z
                (*voxels)[id * 8 + 4] = ox + vx * x + vy * y + vz * (z + stepz);            // x,   y,   z+1
                (*voxels)[id * 8 + 5] = ox + vx * (x + stepx) + vy * y + vz * (z + stepz);  // x+1, y,   z+1
                (*voxels)[id * 8 + 6] = ox + vx * (x + stepx) + vy * (y + stepy) + vz * (z + stepz); // x+1, y+1, z+1
                (*voxels)[id * 8 + 7] = ox + vx * x + vy * (y + stepy) + vz * (z + stepz);  // x,   y+1, z+1
                id++;
            }
        }
    }

    return voxels;
}

StaticVector<int>* createRandomArrayOfIntegers(int n)
{
    /* initialize random seed: */
    srand(time(nullptr));

    StaticVector<int>* tracksPointsRandomIds = new StaticVector<int>(n);
    for(int j = 0; j < n; j++)
    {
        tracksPointsRandomIds->push_back(j);
    }

    for(int j = 0; j < n - 1; j++)
    {
        int rid = rand() % (n - j);

        /*
        if ((j+rid<0)||(j+rid>=tracksPoints->size())) {
                printf("WANRING rid ot of limits %i, 0 to %i !!!! \n",j+rid,tracksPoints->size());
        };
        */

        int v = (*tracksPointsRandomIds)[j + rid];
        (*tracksPointsRandomIds)[j + rid] = (*tracksPointsRandomIds)[j];
        (*tracksPointsRandomIds)[j] = v;
    }

    // test
    /*
    {
            StaticVectorBool *tracksPointsRandomIdsB = new StaticVectorBool(n);
            tracksPointsRandomIdsB->resize_with(n,false);
            for (int k=0;k<n;k++) {
                    int j = (*tracksPointsRandomIds)[k];
                    (*tracksPointsRandomIdsB)[j] = true;
            };


            for (int j=0;j<n;j++) {
                    if ((*tracksPointsRandomIdsB)[j]==false) {
                            printf("WANRING  ((*tracksPointsRandomIdsB)[j]==false) !!!! \n");
                    };
            };


            delete tracksPointsRandomIdsB;
    };
    */

    return tracksPointsRandomIds;
}

float getCGDepthFromSeeds(const MultiViewParams* mp, int rc)
{
    StaticVector<SeedPoint>* seeds;
    loadSeedsFromFile(&seeds, rc, mp->mip, EFileType::seeds);

    float midDepth = -1.0f;

    if(seeds->size() > 20)
    {
        OrientedPoint rcplane;
        rcplane.p = mp->CArr[rc];
        rcplane.n = mp->iRArr[rc] * Point3d(0.0, 0.0, 1.0);
        rcplane.n = rcplane.n.normalize();

        Point3d cg = Point3d(0.0f, 0.0f, 0.0f);
        for(int i = 0; i < seeds->size(); i++)
        {
            SeedPoint* sp = &(*seeds)[i];
            cg = cg + sp->op.p;
        }
        cg = cg / (float)seeds->size();
        midDepth = pointPlaneDistance(cg, rcplane.p, rcplane.n);
    }

    delete seeds;

    return midDepth;
}

int findNSubstrsInString(const std::string& str, const std::string& val)
{
    int last = 0;
    int n = 0;
    int pos = str.find(val, last);
    while(pos > -1)
    {
        n++;
        last = pos + val.length();
        pos = str.find(val, last);
    }
    return n;
}

std::string num2str(int num)
{
    std::stringstream out;
    out << num;
    return out.str();
}

std::string num2str(float num)
{
    std::stringstream out;
    out << num;
    return out.str();
}

std::string num2str(int64_t num)
{
    std::stringstream out;
    out << num;
    return out.str();
}

std::string num2strThreeDigits(int index)
{
    std::string ms;

    if(index < 10)
    {
        ms = "00" + num2str(index);
    }
    else
    {
        if(index < 100)
        {
            ms = "0" + num2str(index);
        }
        else
        {
            ms = num2str(index);
        }
    }

    return ms;
}

std::string num2strFourDecimal(int index)
{
    char tmp[50];
    sprintf(tmp, "%05i", index);
    std::string s = tmp;
    return s;
}

std::string num2strTwoDecimal(int index)
{
    std::string ms;
    if(index < 10)
    {
        ms = "0" + num2str(index);
    }
    else
    {
        if(index < 100)
        {
            ms = num2str(index);
        }
    }

    return ms;
}

} // namespace aliceVision
