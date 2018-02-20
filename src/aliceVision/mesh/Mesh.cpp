// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Mesh.hpp"
#include <aliceVision/structures/geometry.hpp>
#include <aliceVision/structures/OrientedPoint.hpp>
#include <aliceVision/structures/Pixel.hpp>

#include <boost/filesystem.hpp>

#include <fstream>
#include <map>

namespace aliceVision {

namespace bfs = boost::filesystem;

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
    delete pts;
    delete tris;
}

void Mesh::saveToObj(const std::string& filename)
{
  std::cout << "saveToObj: " << filename << std::endl;
  FILE* f = fopen(filename.c_str(), "w");

  fprintf(f, "# \n");
  fprintf(f, "# Wavefront OBJ file\n");
  fprintf(f, "# Created with AliceVision\n");
  fprintf(f, "# \n");
  fprintf(f, "g Mesh\n");
  for(int i = 0; i < pts->size(); i++)
      fprintf(f, "v %f %f %f\n", (*pts)[i].x, (*pts)[i].y, (*pts)[i].z);

  for(int i = 0; i < tris->size(); i++)
  {
      Mesh::triangle& t = (*tris)[i];
      fprintf(f, "f %i %i %i\n", t.i[0] + 1, t.i[1] + 1, t.i[2] + 1);
  }
  fclose(f);
  std::cout << "saveToObj done." << std::endl;
}

bool Mesh::loadFromBin(std::string binFileName)
{
    FILE* f = fopen(binFileName.c_str(), "rb");

    if(f == nullptr)
    {
        return false;
    }

    int npts;
    fread(&npts, sizeof(int), 1, f);
    pts = new StaticVector<Point3d>(npts);
    pts->resize(npts);
    fread(&(*pts)[0], sizeof(Point3d), npts, f);
    int ntris;
    fread(&ntris, sizeof(int), 1, f);
    tris = new StaticVector<Mesh::triangle>(ntris);
    tris->resize(ntris);
    fread(&(*tris)[0], sizeof(Mesh::triangle), ntris, f);
    fclose(f);

    return true;
}

void Mesh::saveToBin(std::string binFileName)
{
    long t = std::clock();
    std::cout << "save mesh to bin." << std::endl;
    // printf("open\n");
    FILE* f = fopen(binFileName.c_str(), "wb");
    int npts = pts->size();
    // printf("write npts %i\n",npts);
    fwrite(&npts, sizeof(int), 1, f);
    // printf("write pts\n");
    fwrite(&(*pts)[0], sizeof(Point3d), npts, f);
    int ntris = tris->size();
    // printf("write ntris %i\n",ntris);
    fwrite(&ntris, sizeof(int), 1, f);
    // printf("write tris\n");
    fwrite(&(*tris)[0], sizeof(Mesh::triangle), ntris, f);
    // printf("close\n");
    fclose(f);
    // printf("done\n");
    printfElapsedTime(t, "Save mesh to bin ");
}

void Mesh::addMesh(Mesh* me)
{
    int npts = sizeOfStaticVector<Point3d>(pts);
    int ntris = sizeOfStaticVector<Mesh::triangle>(tris);
    int npts1 = sizeOfStaticVector<Point3d>(me->pts);
    int ntris1 = sizeOfStaticVector<Mesh::triangle>(me->tris);

    //	printf("pts needed %i of %i allocated\n",npts+npts1,pts->reserved());
    //	printf("tris needed %i of %i allocated\n",ntris+ntris1,tris->reserved());

    if((pts != nullptr) && (tris != nullptr) && (npts + npts1 <= pts->capacity()) &&
       (ntris + ntris1 <= tris->capacity()))
    {
        for(int i = 0; i < npts1; i++)
        {
            pts->push_back((*me->pts)[i]);
        }
        for(int i = 0; i < ntris1; i++)
        {
            Mesh::triangle t = (*me->tris)[i];
            if((t.i[0] >= 0) && (t.i[0] < npts1) && (t.i[1] >= 0) && (t.i[1] < npts1) && (t.i[2] >= 0) &&
               (t.i[2] < npts1))
            {
                t.i[0] += npts;
                t.i[1] += npts;
                t.i[2] += npts;
                tris->push_back(t);
            }
            else
            {
                printf("WARNING BAD TRIANGLE INDEX %i %i %i, npts : %i \n", t.i[0], t.i[1], t.i[2], npts1);
            }
        }
    }
    else
    {
        StaticVector<Point3d>* ptsnew = new StaticVector<Point3d>(npts + npts1);
        StaticVector<Mesh::triangle>* trisnew = new StaticVector<Mesh::triangle>(ntris + ntris1);

        for(int i = 0; i < npts; i++)
        {
            ptsnew->push_back((*pts)[i]);
        }
        for(int i = 0; i < npts1; i++)
        {
            ptsnew->push_back((*me->pts)[i]);
        }

        for(int i = 0; i < ntris; i++)
        {
            trisnew->push_back((*tris)[i]);
        }
        for(int i = 0; i < ntris1; i++)
        {
            Mesh::triangle t = (*me->tris)[i];
            if((t.i[0] >= 0) && (t.i[0] < npts1) && (t.i[1] >= 0) && (t.i[1] < npts1) && (t.i[2] >= 0) &&
               (t.i[2] < npts1))
            {
                t.i[0] += npts;
                t.i[1] += npts;
                t.i[2] += npts;
                trisnew->push_back(t);
            }
            else
            {
                printf("WARNING BAD TRIANGLE INDEX %i %i %i, npts : %i \n", t.i[0], t.i[1], t.i[2], npts1);
            }
        }

        if(pts != nullptr)
        {
            delete pts;
        }
        if(tris != nullptr)
        {
            delete tris;
        }
        pts = ptsnew;
        tris = trisnew;
    }
}

Mesh::triangle_proj Mesh::getTriangleProjection(int triid, const MultiViewParams* mp, int rc, int w, int h) const
{
    int ow = mp->mip->getWidth(rc);
    int oh = mp->mip->getHeight(rc);

    triangle_proj tp;
    for(int j = 0; j < 3; j++)
    {
        mp->getPixelFor3DPoint(&tp.tp2ds[j], (*pts)[(*tris)[triid].i[j]], rc);
        tp.tp2ds[j].x = (tp.tp2ds[j].x / (float)ow) * (float)w;
        tp.tp2ds[j].y = (tp.tp2ds[j].y / (float)oh) * (float)h;
        tp.tpixs[j].x = (int)floor(tp.tp2ds[j].x);
        tp.tpixs[j].y = (int)floor(tp.tp2ds[j].y);
    }

    tp.lu.x = w;
    tp.lu.y = h;
    tp.rd.x = 0;
    tp.rd.y = 0;
    for(int j = 0; j < 3; j++)
    {
        if((float)tp.lu.x > tp.tp2ds[j].x)
        {
            tp.lu.x = (int)tp.tp2ds[j].x;
        }
        if((float)tp.lu.y > tp.tp2ds[j].y)
        {
            tp.lu.y = (int)tp.tp2ds[j].y;
        }
        if((float)tp.rd.x < tp.tp2ds[j].x)
        {
            tp.rd.x = (int)tp.tp2ds[j].x;
        }
        if((float)tp.rd.y < tp.tp2ds[j].y)
        {
            tp.rd.y = (int)tp.tp2ds[j].y;
        }
    }

    return tp;
}

bool Mesh::isTriangleProjectionInImage(Mesh::triangle_proj tp, int w, int h) const
{
    for(int j = 0; j < 3; j++)
    {
        if(!((tp.tpixs[j].x > 0) && (tp.tpixs[j].x < w) && (tp.tpixs[j].y > 0) && (tp.tpixs[j].y < h)))
        {
            return false;
        }
    }
    return true;
}

StaticVector<Point2d>* Mesh::getTrianglePixelIntersectionsAndInternalPoints(Mesh::triangle_proj* tp,
                                                                               Mesh::rectangle* re)
{
    StaticVector<Point2d>* out = new StaticVector<Point2d>(20);

    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[0]))
    {
        out->push_back(re->P[0]);
    }
    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[1]))
    {
        out->push_back(re->P[1]);
    }
    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[2]))
    {
        out->push_back(re->P[2]);
    }
    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[3]))
    {
        out->push_back(re->P[3]);
    }
    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[0])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[0])))
    {
        out->push_back(tp->tp2ds[0]);
    }
    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[1])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[1])))
    {
        out->push_back(tp->tp2ds[1]);
    }
    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[2])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[2])))
    {
        out->push_back(tp->tp2ds[2]);
    }

    Point2d lli;
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[0], &tp->tp2ds[1], &re->P[0], &re->P[1]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[1], &tp->tp2ds[2], &re->P[0], &re->P[1]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[2], &tp->tp2ds[0], &re->P[0], &re->P[1]))
    {
        out->push_back(lli);
    }

    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[0], &tp->tp2ds[1], &re->P[1], &re->P[2]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[1], &tp->tp2ds[2], &re->P[1], &re->P[2]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[2], &tp->tp2ds[0], &re->P[1], &re->P[2]))
    {
        out->push_back(lli);
    }

    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[0], &tp->tp2ds[1], &re->P[2], &re->P[3]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[1], &tp->tp2ds[2], &re->P[2], &re->P[3]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[2], &tp->tp2ds[0], &re->P[2], &re->P[3]))
    {
        out->push_back(lli);
    }

    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[0], &tp->tp2ds[1], &re->P[3], &re->P[0]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[1], &tp->tp2ds[2], &re->P[3], &re->P[0]))
    {
        out->push_back(lli);
    }
    if(lineSegmentsIntersect2DTest(&lli, &tp->tp2ds[2], &tp->tp2ds[0], &re->P[3], &re->P[0]))
    {
        out->push_back(lli);
    }

    return out;
}

StaticVector<Point3d>* Mesh::getTrianglePixelIntersectionsAndInternalPoints(const MultiViewParams* mp, int idTri,
                                                                               Pixel&  /*pix*/, int rc,
                                                                               Mesh::triangle_proj* tp,
                                                                               Mesh::rectangle* re)
{

    Point3d A = (*pts)[(*tris)[idTri].i[0]];
    Point3d B = (*pts)[(*tris)[idTri].i[1]];
    Point3d C = (*pts)[(*tris)[idTri].i[2]];

    StaticVector<Point3d>* out = triangleRectangleIntersection(A, B, C, mp, rc, re->P);

    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[0])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[0])))
    {
        out->push_back(A);
    }
    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[1])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[1])))
    {
        out->push_back(B);
    }
    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[2])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[2])))
    {
        out->push_back(C);
    }

    return out;
}

Point2d Mesh::getTrianglePixelInternalPoint(Mesh::triangle_proj* tp, Mesh::rectangle* re)
{

    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[0])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[0])))
    {
        return tp->tp2ds[0];
    }
    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[1])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[1])))
    {
        return tp->tp2ds[1];
    }
    if((isPointInTriangle(re->P[0], re->P[1], re->P[2], tp->tp2ds[2])) ||
       (isPointInTriangle(re->P[2], re->P[3], re->P[0], tp->tp2ds[2])))
    {
        return tp->tp2ds[2];
    }

    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[0]))
    {
        return re->P[0];
    }
    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[1]))
    {
        return re->P[1];
    }
    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[2]))
    {
        return re->P[2];
    }
    if(isPointInTriangle(tp->tp2ds[0], tp->tp2ds[1], tp->tp2ds[2], re->P[3]))
    {
        return re->P[3];
    }
}

bool Mesh::doesTriangleIntersectsRectangle(Mesh::triangle_proj* tp, Mesh::rectangle* re)
{

    Point2d p1[3];
    Point2d p2[3];
    p1[0] = re->P[0];
    p1[1] = re->P[1];
    p1[2] = re->P[2];
    p2[0] = re->P[2];
    p2[1] = re->P[3];
    p2[2] = re->P[0];

    return ((TrianglesOverlap(tp->tp2ds, p1)) || (TrianglesOverlap(tp->tp2ds, p2)) ||
            (isPointInTriangle(p1[0], p1[1], p1[2], tp->tp2ds[0])) ||
            (isPointInTriangle(p1[0], p1[1], p1[2], tp->tp2ds[1])) ||
            (isPointInTriangle(p1[0], p1[1], p1[2], tp->tp2ds[2])) ||
            (isPointInTriangle(p2[0], p2[1], p2[2], tp->tp2ds[0])) ||
            (isPointInTriangle(p2[0], p2[1], p2[2], tp->tp2ds[1])) ||
            (isPointInTriangle(p2[0], p2[1], p2[2], tp->tp2ds[2])));

    /*
            Point2d p;
            Point2d *_p = &p;

            if ( isPointInTriangle(&re->P[0], &re->P[1], &re->P[2], &tp->tp2ds[0]) ||
                 isPointInTriangle(&re->P[1], &re->P[2], &re->P[0], &tp->tp2ds[0]) ||
                     isPointInTriangle(&re->P[0], &re->P[1], &re->P[2], &tp->tp2ds[1]) ||
                 isPointInTriangle(&re->P[1], &re->P[2], &re->P[0], &tp->tp2ds[1]) ||
                     isPointInTriangle(&re->P[0], &re->P[1], &re->P[2], &tp->tp2ds[2]) ||
                 isPointInTriangle(&re->P[1], &re->P[2], &re->P[0], &tp->tp2ds[2]) )
            {
                    *_p = (tp->tp2ds[0]+tp->tp2ds[1]+tp->tp2ds[2])/3.0;
                    return true;
            };

            Point2d PS = (re->P[0]+re->P[1]+re->P[2]+re->P[3])/4.0;

            if (isPointInTriangle(&tp->tp2ds[0], &tp->tp2ds[1], &tp->tp2ds[2], &PS) == true)
            {
                    *_p = PS;
                    return true;
            };

            if (isPointInTriangle(&tp->tp2ds[0], &tp->tp2ds[1], &tp->tp2ds[2], &re->P[0]) == true)
            {
                    *_p = tp->tp2ds[0];
                    return true;
            };

            if (isPointInTriangle(&tp->tp2ds[0], &tp->tp2ds[1], &tp->tp2ds[2], &re->P[1]) == true)
            {
                    *_p = tp->tp2ds[1];
                    return true;
            };

            if (isPointInTriangle(&tp->tp2ds[0], &tp->tp2ds[1], &tp->tp2ds[2], &re->P[2]) == true)
            {
                    *_p = tp->tp2ds[2];
                    return true;
            };

            if (isPointInTriangle(&tp->tp2ds[0], &tp->tp2ds[1], &tp->tp2ds[2], &re->P[3]) == true)
            {
                    *_p = tp->tp2ds[3];
                    return true;
            };


            if ( lineSegmentsIntersect2DTest(&tp->tp2ds[0],&tp->tp2ds[1],&re->P[0],&re->P[1]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[0],&tp->tp2ds[1],&re->P[1],&re->P[2]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[0],&tp->tp2ds[1],&re->P[2],&re->P[3]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[0],&tp->tp2ds[1],&re->P[3],&re->P[0]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[1],&tp->tp2ds[2],&re->P[0],&re->P[1]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[1],&tp->tp2ds[2],&re->P[1],&re->P[2]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[1],&tp->tp2ds[2],&re->P[2],&re->P[3]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[1],&tp->tp2ds[2],&re->P[3],&re->P[0]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[2],&tp->tp2ds[0],&re->P[0],&re->P[1]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[2],&tp->tp2ds[0],&re->P[1],&re->P[2]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[2],&tp->tp2ds[0],&re->P[2],&re->P[3]) ||
                     lineSegmentsIntersect2DTest(&tp->tp2ds[2],&tp->tp2ds[0],&re->P[3],&re->P[0]) )
            {
                    return true;
            };


            return false;
    */
}

StaticVector<StaticVector<int>*>* Mesh::getPtsNeighTris()
{
    StaticVector<Voxel>* tirsPtsIds = new StaticVector<Voxel>(tris->size() * 3);
    for(int i = 0; i < tris->size(); i++)
    {
        tirsPtsIds->push_back(Voxel((*tris)[i].i[0], i, 0));
        tirsPtsIds->push_back(Voxel((*tris)[i].i[1], i, 0));
        tirsPtsIds->push_back(Voxel((*tris)[i].i[2], i, 0));
    }
    qsort(&(*tirsPtsIds)[0], tirsPtsIds->size(), sizeof(Voxel), qSortCompareVoxelByXAsc);

    int i = 0;
    int j = 0;
    int k = 0;
    int firstid = 0;
    while(i < tirsPtsIds->size())
    {
        k++;
        (*tirsPtsIds)[i].z = j;
        if((i == tirsPtsIds->size() - 1) || ((*tirsPtsIds)[i].x != (*tirsPtsIds)[i + 1].x))
        {
            (*tirsPtsIds)[firstid].z = k;
            j++;
            firstid = i + 1;
            k = 0;
        }
        i++;
    }
    int npts = j;

    StaticVector<StaticVector<int>*>* ptsNeighTris = new StaticVector<StaticVector<int>*>(pts->size());
    ptsNeighTris->resize_with(pts->size(), nullptr);

    i = 0;
    for(j = 0; j < npts; j++)
    {
        int middlePtId = (*tirsPtsIds)[i].x;
        int numincidenttris = (*tirsPtsIds)[i].z;
        int i0 = i;
        int i1 = i + numincidenttris;
        i = i1;

        StaticVector<int>* triTmp = new StaticVector<int>(numincidenttris);
        for(int l = i0; l < i1; l++)
        {
            triTmp->push_back((*tirsPtsIds)[l].y);
        }

        (*ptsNeighTris)[middlePtId] = triTmp;
    }

    delete tirsPtsIds;

    return ptsNeighTris;
}

StaticVector<StaticVector<int>*>* Mesh::getPtsNeighPtsOrdered()
{
    StaticVector<StaticVector<int>*>* ptsNeighTris = getPtsNeighTris();

    StaticVector<StaticVector<int>*>* ptsNeighPts = new StaticVector<StaticVector<int>*>(pts->size());
    ptsNeighPts->resize_with(pts->size(), nullptr);

    for(int i = 0; i < pts->size(); i++)
    {
        int middlePtId = i;
        StaticVector<int>* triTmp = (*ptsNeighTris)[i];
        if((triTmp != nullptr) && (triTmp->size() > 0))
        {
            StaticVector<int>* vhid = new StaticVector<int>(triTmp->size() * 2);
            int acttriptid = (*tris)[(*triTmp)[0]].i[0];
            int firsttriptid = acttriptid;
            vhid->push_back(acttriptid);

            bool isThereTWithActtriptid = true;
            while((triTmp->size() > 0) && (isThereTWithActtriptid))
            {
                isThereTWithActtriptid = false;

                // find tri with middlePtId and acttriptid and get remainnig point id
                int l1 = 0;
                while((l1 < triTmp->size()) && (!isThereTWithActtriptid))
                {
                    bool okmiddlePtId = false;
                    bool okacttriptid = false;
                    int remptid = -1; // remaining pt id
                    for(int k = 0; k < 3; k++)
                    {
                        int triptid = (*tris)[(*triTmp)[l1]].i[k];
                        float len = ((*pts)[middlePtId] - (*pts)[triptid]).size();
                        if((triptid != middlePtId) && (triptid != acttriptid) && (len > 0.0) && (!(len != len)) &&
                           (!std::isnan(len)))
                        {
                            remptid = triptid;
                        }
                        if(triptid == middlePtId)
                        {
                            okmiddlePtId = true;
                        }
                        if(triptid == acttriptid)
                        {
                            okacttriptid = true;
                        }
                    }

                    if((okmiddlePtId) && (okacttriptid) && (remptid > -1))
                    {
                        acttriptid = remptid;
                        triTmp->remove(l1);
                        vhid->push_back(acttriptid);
                        isThereTWithActtriptid = true;
                    }
                    l1++;
                }
            } // while (triTmp->size()>0)

            // if ((acttriptid != firsttriptid)||(vhid->size()<3)) {
            //	delete vhid;
            //}else{
            if(vhid->size() == 0)
            {
                delete vhid;
            }
            else
            {
                if(acttriptid == firsttriptid)
                {
                    vhid->pop(); // remove last ... which ist first
                }

                // remove duplicities
                StaticVector<int>* vhid1 = new StaticVector<int>(vhid->size());
                for(int k1 = 0; k1 < vhid->size(); k1++)
                {
                    if(vhid1->indexOf((*vhid)[k1]) == -1)
                    {
                        vhid1->push_back((*vhid)[k1]);
                    }
                }
                delete vhid;

                (*ptsNeighPts)[middlePtId] = vhid1;

                /*
                FILE *fm=fopen("fans.m","w");

                fprintf(fm,"clear all \n close all \n clc; \n figure; \n hold on \n axis equal \n");

                for (int l=0;l<vhid->size();l++) {
                        Point3d p = (*pts)[middlePtId];
                        Point3d np = (*pts)[(*vhid)[l]];
                        fprintf(fm,"plot3([%f %f],[%f %f],[%f %f],'r-');\n",np.x,p.x,np.y,p.y,np.z,p.z);
                        fprintf(fm,"text(%f,%f,%f,'%i');\n",np.x,np.y,np.z,l);
                };
                for (int l=0;l<vhid->size()-1;l++) {
                        Point3d p = (*pts)[(*vhid)[l]];
                        Point3d np = (*pts)[(*vhid)[l+1]];
                        fprintf(fm,"plot3([%f %f],[%f %f],[%f %f],'b-');\n",np.x,p.x,np.y,p.y,np.z,p.z);
                };
                Point3d p = (*pts)[(*vhid)[0]];
                Point3d np = (*pts)[(*vhid)[vhid->size()-1]];
                fprintf(fm,"plot3([%f %f],[%f %f],[%f %f],'b-');\n",np.x,p.x,np.y,p.y,np.z,p.z);

                fclose(fm);
                */
            }
        }
    }

    deleteArrayOfArrays<int>(&ptsNeighTris);

    return ptsNeighPts;
}

StaticVector<StaticVector<int>*>* Mesh::getTrisMap(const MultiViewParams* mp, int rc, int  /*scale*/, int w, int h)
{
    long tstart = clock();
    printf("getTrisMap \n");
    StaticVector<int>* nmap = new StaticVector<int>(w * h);
    nmap->resize_with(w * h, 0);

    printf("estimating numbers \n");
    long t1 = initEstimate();
    for(int i = 0; i < tris->size(); i++)
    {
        triangle_proj tp = getTriangleProjection(i, mp, rc, w, h);
        if((isTriangleProjectionInImage(tp, w, h)))
        {
            Pixel pix;
            for(pix.x = tp.lu.x; pix.x <= tp.rd.x; pix.x++)
            {
                for(pix.y = tp.lu.y; pix.y <= tp.rd.y; pix.y++)
                {
                    Mesh::rectangle re = Mesh::rectangle(pix, 1);
                    if(doesTriangleIntersectsRectangle(&tp, &re))
                    {
                        (*nmap)[pix.x * h + pix.y] += 1;
                    }
                } // for y
            }     // for x
        }         // isthere
        printfEstimate(i, tris->size(), t1);
    } // for i ntris
    finishEstimate();

    // allocate
    printf("allocating\n");
    StaticVector<StaticVector<int>*>* tmp = new StaticVector<StaticVector<int>*>(w * h);
    tmp->resize_with(w * h, nullptr);
    StaticVector<int>** ptmp = &(*tmp)[0];
    for(int i = 0; i < w * h; i++)
    {
        if((*nmap)[i] > 0)
        {
            *ptmp = new StaticVector<int>((*nmap)[i]);
        }
        ptmp++;
    }
    delete nmap;

    // fill
    printf("filling\n");
    t1 = initEstimate();
    for(int i = 0; i < tris->size(); i++)
    {
        triangle_proj tp = getTriangleProjection(i, mp, rc, w, h);
        if((isTriangleProjectionInImage(tp, w, h)))
        {
            Pixel pix;
            for(pix.x = tp.lu.x; pix.x <= tp.rd.x; pix.x++)
            {
                for(pix.y = tp.lu.y; pix.y <= tp.rd.y; pix.y++)
                {
                    Mesh::rectangle re = Mesh::rectangle(pix, 1);
                    if(doesTriangleIntersectsRectangle(&tp, &re))
                    {
                        (*tmp)[pix.x * h + pix.y]->push_back(i);
                    }
                } // for y
            }     // for x
        }         // isthere
        printfEstimate(i, tris->size(), t1);
    } // for i ntris
    finishEstimate();

    printfElapsedTime(tstart);

    return tmp;
}

StaticVector<StaticVector<int>*>* Mesh::getTrisMap(StaticVector<int>* visTris, const MultiViewParams* mp, int rc,
                                                      int  /*scale*/, int w, int h)
{
    long tstart = clock();
    printf("getTrisMap \n");
    StaticVector<int>* nmap = new StaticVector<int>(w * h);
    nmap->resize_with(w * h, 0);

    printf("estimating numbers \n");
    long t1 = initEstimate();
    for(int m = 0; m < visTris->size(); m++)
    {
        int i = (*visTris)[m];
        triangle_proj tp = getTriangleProjection(i, mp, rc, w, h);
        if((isTriangleProjectionInImage(tp, w, h)))
        {
            Pixel pix;
            for(pix.x = tp.lu.x; pix.x <= tp.rd.x; pix.x++)
            {
                for(pix.y = tp.lu.y; pix.y <= tp.rd.y; pix.y++)
                {
                    Mesh::rectangle re = Mesh::rectangle(pix, 1);
                    if(doesTriangleIntersectsRectangle(&tp, &re))
                    {
                        (*nmap)[pix.x * h + pix.y] += 1;
                    }
                } // for y
            }     // for x
        }         // isthere
        printfEstimate(i, tris->size(), t1);
    } // for i ntris
    finishEstimate();

    // allocate
    printf("allocating\n");
    StaticVector<StaticVector<int>*>* tmp = new StaticVector<StaticVector<int>*>(w * h);
    tmp->resize_with(w * h, nullptr);
    StaticVector<int>** ptmp = &(*tmp)[0];
    for(int i = 0; i < w * h; i++)
    {
        if((*nmap)[i] > 0)
        {
            *ptmp = new StaticVector<int>((*nmap)[i]);
        }
        ptmp++;
    }
    delete nmap;

    // fill
    printf("filling\n");
    t1 = initEstimate();
    for(int m = 0; m < visTris->size(); m++)
    {
        int i = (*visTris)[m];
        triangle_proj tp = getTriangleProjection(i, mp, rc, w, h);
        if((isTriangleProjectionInImage(tp, w, h)))
        {
            Pixel pix;
            for(pix.x = tp.lu.x; pix.x <= tp.rd.x; pix.x++)
            {
                for(pix.y = tp.lu.y; pix.y <= tp.rd.y; pix.y++)
                {
                    Mesh::rectangle re = Mesh::rectangle(pix, 1);
                    if(doesTriangleIntersectsRectangle(&tp, &re))
                    {
                        (*tmp)[pix.x * h + pix.y]->push_back(i);
                    }
                } // for y
            }     // for x
        }         // isthere
        printfEstimate(i, tris->size(), t1);
    } // for i ntris
    finishEstimate();

    printfElapsedTime(tstart);

    return tmp;
}

void Mesh::getDepthMap(StaticVector<float>* depthMap, const MultiViewParams* mp, int rc, int scale, int w, int h)
{
    StaticVector<StaticVector<int>*>* tmp = getTrisMap(mp, rc, scale, w, h);
    getDepthMap(depthMap, tmp, mp, rc, scale, w, h);
    deleteArrayOfArrays<int>(&tmp);
}

void Mesh::getDepthMap(StaticVector<float>* depthMap, StaticVector<StaticVector<int>*>* tmp, const MultiViewParams* mp,
                          int rc, int scale, int w, int h)
{
    depthMap->resize_with(w * h, -1.0f);

    Pixel pix;
    for(pix.x = 0; pix.x < w; pix.x++)
    {
        for(pix.y = 0; pix.y < h; pix.y++)
        {

            StaticVector<int>* ti = (*tmp)[pix.x * h + pix.y];
            if((ti != nullptr) && (ti->size() > 0))
            {
                Point2d p;
                p.x = (double)pix.x;
                p.y = (double)pix.y;

                double mindepth = 10000000.0;

                for(int i = 0; i < ti->size(); i++)
                {
                    int idTri = (*ti)[i];
                    OrientedPoint tri;
                    tri.p = (*pts)[(*tris)[idTri].i[0]];
                    tri.n = cross(((*pts)[(*tris)[idTri].i[1]] - (*pts)[(*tris)[idTri].i[0]]).normalize(),
                                  ((*pts)[(*tris)[idTri].i[2]] - (*pts)[(*tris)[idTri].i[0]]).normalize());

                    Mesh::rectangle re = Mesh::rectangle(pix, 1);
                    triangle_proj tp = getTriangleProjection(idTri, mp, rc, w, h);

                    /*
                    if ((pix.x==66)&&(pix.y==117))
                    {
                            printf("plot([%f %f],[%f %f],'r-');\n",
                                    tp.tp2ds[0].x,tp.tp2ds[1].x,tp.tp2ds[0].y,tp.tp2ds[1].y);
                            printf("plot([%f %f],[%f %f],'r-');\n",
                                    tp.tp2ds[1].x,tp.tp2ds[2].x,tp.tp2ds[1].y,tp.tp2ds[2].y);
                            printf("plot([%f %f],[%f %f],'r-');\n",
                                    tp.tp2ds[2].x,tp.tp2ds[0].x,tp.tp2ds[2].y,tp.tp2ds[0].y);
                    };


                    if ((pix.x==67)&&(pix.y==118))
                    {
                            printf("%f\n", angleBetwV1andV2((mp->CArr[rc]-tri.p).normalize(),tri.n));
                    }
                    */

                    StaticVector<Point2d>* tpis = getTrianglePixelIntersectionsAndInternalPoints(&tp, &re);

                    double maxd = -1.0;
                    for(int k = 0; k < tpis->size(); k++)
                    {
                        Point3d lpi = linePlaneIntersect(
                            mp->CArr[rc], (mp->iCamArr[rc] * ((*tpis)[k] * (float)scale)).normalize(), tri.p, tri.n);
                        if(!std::isnan(angleBetwV1andV2((mp->CArr[rc] - tri.p).normalize(), tri.n)))
                        {
                            maxd = std::max(maxd, (mp->CArr[rc] - lpi).size());
                        }
                        else
                        {
                            /*
                            printf("A %f %f %f\n", (*pts)[(*tris)[idTri].i[0]].x, (*pts)[(*tris)[idTri].i[0]].y,
                            (*pts)[(*tris)[idTri].i[0]].z);
                            printf("B %f %f %f\n", (*pts)[(*tris)[idTri].i[1]].x, (*pts)[(*tris)[idTri].i[1]].y,
                            (*pts)[(*tris)[idTri].i[1]].z);
                            printf("C %f %f %f\n", (*pts)[(*tris)[idTri].i[2]].x, (*pts)[(*tris)[idTri].i[2]].y,
                            (*pts)[(*tris)[idTri].i[2]].z);
                            printf("n %f %f %f\n", tri.n.x, tri.n.y, tri.n.z);
                            printf("ids %i %i %i\n", (*tris)[idTri].i[0], (*tris)[idTri].i[1], (*tris)[idTri].i[2]);
                            */

                            maxd = std::max(maxd, (mp->CArr[rc] - (*pts)[(*tris)[idTri].i[1]]).size());

                            /*
                            StaticVector<Point3d> *tpis1 = getTrianglePixelIntersectionsAndInternalPoints(mp, idTri,
                            pix, rc, &tp, &re);

                            float maxd = -1.0;
                            for (int k=0;k<tpis1->size();k++) {
                                    maxd = std::max(maxd,(mp->CArr[rc]-(*tpis1)[k]).size());
                            };

                            delete tpis1;
                            */
                        }
                        //};
                    }
                    mindepth = std::min(mindepth, maxd);

                    delete tpis;
                }

                /*
                //for (int idTri=0;idTri<tris->size();idTri++)
                for (int i=0;i<ti->size();i++)
                {
                        int idTri = (*ti)[i];
                        orientedPoint tri;
                        tri.p = (*pts)[(*tris)[idTri].i[0]];
                        tri.n = cross( ((*pts)[(*tris)[idTri].i[1]]-(*pts)[(*tris)[idTri].i[0]]).normalize(),
                                                   ((*pts)[(*tris)[idTri].i[2]]-(*pts)[(*tris)[idTri].i[0]]).normalize()
                );
                        Point3d lpi =
                linePlaneIntersect(mp->CArr[rc],(mp->iCamArr[rc]*Point2d(p.x,p.y)).normalize(),tri.p,tri.n);
                        if ((mp->is3DPointInFrontOfCam(&lpi,rc)==true)&&
                                (isLineInTriangle(
                                        &(*pts)[(*tris)[idTri].i[0]],
                                        &(*pts)[(*tris)[idTri].i[1]],
                                        &(*pts)[(*tris)[idTri].i[2]],
                                        &mp->CArr[rc],
                                        &(mp->iCamArr[rc]*Point2d(p.x,p.y)).normalize())==true))
                        {
                                mindepth=std::min(mindepth,(mp->CArr[rc]-lpi).size());
                        };
                };
                */

                (*depthMap)[pix.x * h + pix.y] = mindepth;
            }
            else
            {
                (*depthMap)[pix.x * h + pix.y] = -1.0f;
            }
        } // for pix.y
    }     // for pix.x
}

StaticVector<int>* Mesh::getVisibleTrianglesIndexes(std::string depthMapFileName, std::string trisMapFileName,
                                                       const MultiViewParams* mp, int rc, int w, int h)
{
    StaticVector<float>* depthMap = loadArrayFromFile<float>(depthMapFileName);
    StaticVector<StaticVector<int>*>* trisMap = loadArrayOfArraysFromFile<int>(trisMapFileName);

    StaticVector<int>* vistri = getVisibleTrianglesIndexes(trisMap, depthMap, mp, rc, w, h);

    deleteArrayOfArrays<int>(&trisMap);
    delete depthMap;

    return vistri;
}

StaticVector<int>* Mesh::getVisibleTrianglesIndexes(std::string tmpDir, const MultiViewParams* mp, int rc, int w, int h)
{
    std::string depthMapFileName = tmpDir + "depthMap" + std::to_string(mp->mip->getViewId(rc)) + ".bin";
    std::string trisMapFileName = tmpDir + "trisMap" + std::to_string(mp->mip->getViewId(rc)) + ".bin";

    StaticVector<float>* depthMap = loadArrayFromFile<float>(depthMapFileName);
    StaticVector<StaticVector<int>*>* trisMap = loadArrayOfArraysFromFile<int>(trisMapFileName);

    StaticVector<int>* vistri = getVisibleTrianglesIndexes(trisMap, depthMap, mp, rc, w, h);

    deleteArrayOfArrays<int>(&trisMap);
    delete depthMap;

    return vistri;
}

StaticVector<int>* Mesh::getVisibleTrianglesIndexes(StaticVector<float>* depthMap, const MultiViewParams* mp, int rc,
                                                       int w, int h)
{
    int ow = mp->mip->getWidth(rc);
    int oh = mp->mip->getHeight(rc);

    StaticVector<int>* out = new StaticVector<int>(tris->size());
    for(int i = 0; i < tris->size(); i++)
    {
        Point3d cg = computeTriangleCenterOfGravity(i);
        Pixel pix;
        mp->getPixelFor3DPoint(&pix, cg, rc);
        if(mp->isPixelInImage(pix, 1, rc))
        {
            pix.x = (int)(((float)pix.x / (float)ow) * (float)w);
            pix.y = (int)(((float)pix.y / (float)oh) * (float)h);
            float depth = (*depthMap)[pix.x * h + pix.y];
            float pixSize = mp->getCamPixelSize(cg, rc) * 6.0f;
            // if (depth-pixSize<(mp->CArr[rc]-cg).size()) {
            if(fabs(depth - (mp->CArr[rc] - cg).size()) < pixSize)
            {
                out->push_back(i);
            }
        }
    }
    return out;
}

StaticVector<int>* Mesh::getVisibleTrianglesIndexes(StaticVector<StaticVector<int>*>* trisMap,
                                                       StaticVector<float>* depthMap, const MultiViewParams* mp, int rc,
                                                       int w, int h)
{
    int ow = mp->mip->getWidth(rc);
    int oh = mp->mip->getHeight(rc);

    StaticVectorBool* btris = new StaticVectorBool(tris->size());
    btris->resize_with(tris->size(), false);

    Pixel pix;
    for(pix.x = 0; pix.x < w; pix.x++)
    {
        for(pix.y = 0; pix.y < h; pix.y++)
        {

            StaticVector<int>* ti = (*trisMap)[pix.x * h + pix.y];
            if(ti != nullptr)
            {
                Point2d p;
                p.x = (float)pix.x;
                p.y = (float)pix.y;

                float depth = (*depthMap)[pix.x * h + pix.y];
                for(int i = 0; i < ti->size(); i++)
                {
                    int idTri = (*ti)[i];
                    OrientedPoint tri;
                    tri.p = (*pts)[(*tris)[idTri].i[0]];
                    tri.n = cross(((*pts)[(*tris)[idTri].i[1]] - (*pts)[(*tris)[idTri].i[0]]).normalize(),
                                  ((*pts)[(*tris)[idTri].i[2]] - (*pts)[(*tris)[idTri].i[0]]).normalize());

                    Mesh::rectangle re = Mesh::rectangle(pix, 1);
                    triangle_proj tp = getTriangleProjection(idTri, mp, rc, w, h);

                    /*
                    StaticVector<Point2d> *tpis = getTrianglePixelIntersectionsAndInternalPoints(&tp, &re);
                    float mindepth = 10000000.0f;
                    Point3d minlpi;
                    for (int k=0;k<tpis->size();k++) {
                            Point3d lpi =
                    linePlaneIntersect(mp->CArr[rc],(mp->iCamArr[rc]*((*tpis)[k]*(float)scale)).normalize(),tri.p,tri.n);
                            if (mindepth>(mp->CArr[rc]-lpi).size()) {
                                    mindepth = (mp->CArr[rc]-lpi).size();
                                    minlpi = lpi;
                            };
                    };
                    float pixSize = mp->getCamPixelSize(minlpi,rc) * 2.0f;
                    if (fabs(depth-mindepth)<pixSize) {
                            (*btris)[idTri] = true;
                    };
                    delete tpis;
                    */

                    Point2d tpip = getTrianglePixelInternalPoint(&tp, &re);
                    tpip.x = (tpip.x / (float)w) * (float)ow;
                    tpip.y = (tpip.y / (float)h) * (float)oh;

                    Point3d lpi = linePlaneIntersect(mp->CArr[rc], (mp->iCamArr[rc] * tpip).normalize(), tri.p, tri.n);
                    float lpidepth = (mp->CArr[rc] - lpi).size();
                    float pixSize = mp->getCamPixelSize(lpi, rc) * 2.0f;
                    if(fabs(depth - lpidepth) < pixSize)
                    {
                        (*btris)[idTri] = true;
                    }
                }
            }
        } // for pix.y
    }     // for pix.x

    int nvistris = 0;
    for(int i = 0; i < btris->size(); i++)
    {
        if((*btris)[i])
        {
            nvistris++;
        }
    }

    StaticVector<int>* out = new StaticVector<int>(nvistris);
    for(int i = 0; i < btris->size(); i++)
    {
        if((*btris)[i])
        {
            out->push_back(i);
        }
    }

    // deallocate
    delete btris;

    return out;
}

Mesh* Mesh::generateMeshFromTrianglesSubset(const StaticVector<int>& visTris, StaticVector<int>** out_ptIdToNewPtId) const
{
    Mesh* outMesh = new Mesh();

    StaticVector<int> newIndexPerInputPts;
    newIndexPerInputPts.resize_with(pts->size(), -1); // -1 means unused
    for(int i = 0; i < visTris.size(); i++)
    {
        int idTri = visTris[i];
        newIndexPerInputPts[(*tris)[idTri].i[0]] = 0; // 0 means used
        newIndexPerInputPts[(*tris)[idTri].i[1]] = 0;
        newIndexPerInputPts[(*tris)[idTri].i[2]] = 0;
    }

    int j = 0;
    for(int i = 0; i < pts->size(); i++)
    {
        if(newIndexPerInputPts[i] == 0) // if input point used
        {
            newIndexPerInputPts[i] = j;
            ++j;
        }
    }

    outMesh->pts = new StaticVector<Point3d>(j);

    for(int i = 0; i < pts->size(); i++)
    {
        if(newIndexPerInputPts[i] > -1)
        {
            outMesh->pts->push_back((*pts)[i]);
        }
    }

    outMesh->tris = new StaticVector<Mesh::triangle>(visTris.size());
    for(int i = 0; i < visTris.size(); i++)
    {
        int idTri = visTris[i];
        Mesh::triangle t;
        t.alive = true;
        t.i[0] = newIndexPerInputPts[(*tris)[idTri].i[0]];
        t.i[1] = newIndexPerInputPts[(*tris)[idTri].i[1]];
        t.i[2] = newIndexPerInputPts[(*tris)[idTri].i[2]];
        outMesh->tris->push_back(t);
    }

    if(out_ptIdToNewPtId != nullptr)
    {
        (*out_ptIdToNewPtId) = new StaticVector<int>();
        (*out_ptIdToNewPtId)->swap(newIndexPerInputPts);
    }

    return outMesh;
}

void Mesh::getNotOrientedEdges(StaticVector<StaticVector<int>*>** edgesNeighTris,
                                  StaticVector<Pixel>** edgesPointsPairs)
{
    // printf("getNotOrientedEdges\n");
    StaticVector<Voxel>* edges = new StaticVector<Voxel>(tris->size() * 3);

    for(int i = 0; i < tris->size(); i++)
    {
        int a = (*tris)[i].i[0];
        int b = (*tris)[i].i[1];
        int c = (*tris)[i].i[2];
        edges->push_back(Voxel(std::min(a, b), std::max(a, b), i));
        edges->push_back(Voxel(std::min(b, c), std::max(b, c), i));
        edges->push_back(Voxel(std::min(c, a), std::max(c, a), i));
    }

    qsort(&(*edges)[0], edges->size(), sizeof(Voxel), qSortCompareVoxelByXAsc);

    StaticVector<StaticVector<int>*>* _edgesNeighTris = new StaticVector<StaticVector<int>*>(tris->size() * 3);
    StaticVector<Pixel>* _edgesPointsPairs = new StaticVector<Pixel>(tris->size() * 3);

    // remove duplicities
    int i0 = 0;
    long t1 = initEstimate();
    for(int i = 0; i < edges->size(); i++)
    {
        if((i == edges->size() - 1) || ((*edges)[i].x != (*edges)[i + 1].x))
        {
            StaticVector<Voxel>* edges1 = new StaticVector<Voxel>(i - i0 + 1);
            for(int j = i0; j <= i; j++)
            {
                edges1->push_back((*edges)[j]);
            }
            qsort(&(*edges1)[0], edges1->size(), sizeof(Voxel), qSortCompareVoxelByYAsc);

            int j0 = 0;
            for(int j = 0; j < edges1->size(); j++)
            {
                if((j == edges1->size() - 1) || ((*edges1)[j].y != (*edges1)[j + 1].y))
                {
                    _edgesPointsPairs->push_back(Pixel((*edges1)[j].x, (*edges1)[j].y));
                    StaticVector<int>* neighTris = new StaticVector<int>(j - j0 + 1);
                    for(int k = j0; k <= j; k++)
                    {
                        neighTris->push_back((*edges1)[k].z);
                    }
                    _edgesNeighTris->push_back(neighTris);
                    j0 = j + 1;
                }
            }

            delete edges1;
            i0 = i + 1;
        }

        printfEstimate(i, edges->size(), t1);
    }
    finishEstimate();

    (*edgesNeighTris) = _edgesNeighTris;
    (*edgesPointsPairs) = _edgesPointsPairs;

    delete edges;
}

StaticVector<Point3d>* Mesh::getLaplacianSmoothingVectors(StaticVector<StaticVector<int>*>* ptsNeighPts,
                                                             double maximalNeighDist)
{
    StaticVector<Point3d>* nms = new StaticVector<Point3d>(pts->size());

    for(int i = 0; i < pts->size(); i++)
    {
        Point3d p = (*pts)[i];
        StaticVector<int>* nei = (*ptsNeighPts)[i];
        int nneighs = 0;
        if(nei != nullptr)
        {
            nneighs = nei->size();
        }

        if(nneighs == 0)
        {
            nms->push_back(Point3d(0.0, 0.0, 0.0));
        }
        else
        {
            double maxNeighDist = 0.0f;
            // laplacian smoothing vector
            Point3d n = Point3d(0.0, 0.0, 0.0);
            for(int j = 0; j < nneighs; j++)
            {
                n = n + (*pts)[(*nei)[j]];
                maxNeighDist = std::max(maxNeighDist, (p - (*pts)[(*nei)[j]]).size());
            }
            n = ((n / (float)nneighs) - p);

            float d = n.size();
            n = n.normalize();

            if(std::isnan(d) || std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.z) || (d != d) || (n.x != n.x) ||
               (n.y != n.y) || (n.z != n.z)) // check if is not NaN
            {
                n = Point3d(0.0, 0.0, 0.0);
            }
            else
            {
                n = n * d;
            }

            if(std::isnan(d) || std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.z) || (d != d) || (n.x != n.x) ||
               (n.y != n.y) || (n.z != n.z)) // check if is not NaN
            {
                n = Point3d(0.0, 0.0, 0.0);
            }

            if((maximalNeighDist > 0.0f) && (maxNeighDist > maximalNeighDist))
            {
                n = Point3d(0.0, 0.0, 0.0);
            }

            nms->push_back(n);
        }
    }

    return nms;
}

void Mesh::laplacianSmoothPts(float maximalNeighDist)
{
    StaticVector<StaticVector<int>*>* ptsNei = getPtsNeighPtsOrdered();
    laplacianSmoothPts(ptsNei, maximalNeighDist);
    deleteArrayOfArrays<int>(&ptsNei);
}

void Mesh::laplacianSmoothPts(StaticVector<StaticVector<int>*>* ptsNeighPts, double maximalNeighDist)
{
    StaticVector<Point3d>* nms = getLaplacianSmoothingVectors(ptsNeighPts, maximalNeighDist);

    // smooth
    for(int i = 0; i < pts->size(); i++)
    {
        (*pts)[i] = (*pts)[i] + (*nms)[i];
    }

    delete nms;
}

Point3d Mesh::computeTriangleNormal(int idTri)
{
    return cross(((*pts)[(*tris)[idTri].i[1]] - (*pts)[(*tris)[idTri].i[0]]).normalize(),
                 ((*pts)[(*tris)[idTri].i[2]] - (*pts)[(*tris)[idTri].i[0]]).normalize())
        .normalize();
}

Point3d Mesh::computeTriangleCenterOfGravity(int idTri) const
{
    return ((*pts)[(*tris)[idTri].i[0]] + (*pts)[(*tris)[idTri].i[1]] + (*pts)[(*tris)[idTri].i[2]]) / 3.0f;
}

float Mesh::computeTriangleMaxEdgeLength(int idTri) const
{
    return std::max(std::max(((*pts)[(*tris)[idTri].i[0]] - (*pts)[(*tris)[idTri].i[1]]).size(),
                             ((*pts)[(*tris)[idTri].i[1]] - (*pts)[(*tris)[idTri].i[2]]).size()),
                    ((*pts)[(*tris)[idTri].i[2]] - (*pts)[(*tris)[idTri].i[0]]).size());
}

StaticVector<Point3d>* Mesh::computeNormalsForPts()
{
    StaticVector<StaticVector<int>*>* ptsNeighTris = getPtsNeighTris();
    StaticVector<Point3d>* nms = computeNormalsForPts(ptsNeighTris);
    deleteArrayOfArrays<int>(&ptsNeighTris);
    return nms;
}

StaticVector<Point3d>* Mesh::computeNormalsForPts(StaticVector<StaticVector<int>*>* ptsNeighTris)
{
    StaticVector<Point3d>* nms = new StaticVector<Point3d>(pts->size());
    nms->resize_with(pts->size(), Point3d(0.0f, 0.0f, 0.0f));

    for(int i = 0; i < pts->size(); i++)
    {
        StaticVector<int>* triTmp = (*ptsNeighTris)[i];
        if((triTmp != nullptr) && (triTmp->size() > 0))
        {
            Point3d n = Point3d(0.0f, 0.0f, 0.0f);
            float nn = 0.0f;
            for(int j = 0; j < triTmp->size(); j++)
            {
                Point3d n1 = computeTriangleNormal((*triTmp)[j]);
                n1 = n1.normalize();
                if(std::isnan(n1.x) || std::isnan(n1.y) || std::isnan(n1.z) || (n1.x != n1.x) || (n1.y != n1.y) ||
                   (n1.z != n1.z)) // check if is not NaN
                {
                    //
                }
                else
                {
                    n = n + computeTriangleNormal((*triTmp)[j]);
                    nn += 1.0f;
                }
            }
            n = n / nn;

            n = n.normalize();
            if(std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.z) || (n.x != n.x) || (n.y != n.y) ||
               (n.z != n.z)) // check if is not NaN
            {
                n = Point3d(0.0f, 0.0f, 0.0f);
                printf(".");
            }

            (*nms)[i] = n;
        }
    }

    return nms;
}

void Mesh::smoothNormals(StaticVector<Point3d>* nms, StaticVector<StaticVector<int>*>* ptsNeighPts)
{
    StaticVector<Point3d>* nmss = new StaticVector<Point3d>(pts->size());
    nmss->resize_with(pts->size(), Point3d(0.0f, 0.0f, 0.0f));

    for(int i = 0; i < pts->size(); i++)
    {
        Point3d n = (*nms)[i];
        for(int j = 0; j < sizeOfStaticVector<int>((*ptsNeighPts)[i]); j++)
        {
            n = n + (*nms)[(*(*ptsNeighPts)[i])[j]];
        }
        if(sizeOfStaticVector<int>((*ptsNeighPts)[i]) > 0)
        {
            n = n / (float)sizeOfStaticVector<int>((*ptsNeighPts)[i]);
        }
        n = n.normalize();
        if(std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.z) || (n.x != n.x) || (n.y != n.y) || (n.z != n.z))
        {
            n = Point3d(0.0f, 0.0f, 0.0f);
        }
        (*nmss)[i] = n;
    }
    for(int i = 0; i < pts->size(); i++)
    {
        (*nms)[i] = (*nmss)[i];
    }
    delete nmss;
}

void Mesh::removeFreePointsFromMesh(StaticVector<int>** out_ptIdToNewPtId)
{
    printf("removeFreePointsFromMesh\n");

    // declare all triangles as used
    StaticVector<int> visTris;
    visTris.reserve(tris->size());
    for(int i = 0; i < tris->size(); ++i)
    {
        visTris.push_back(i);
    }
    // generate a new mesh from all triangles so all unused points will be removed
    Mesh* cleanedMesh = generateMeshFromTrianglesSubset(visTris, out_ptIdToNewPtId);

    std::swap(cleanedMesh->pts, pts);
    std::swap(cleanedMesh->tris, tris);

    delete cleanedMesh;
}

float Mesh::computeTriangleProjectionArea(triangle_proj& tp)
{
    // return (float)((tp.rd.x-tp.lu.x+1)*(tp.rd.y-tp.lu.y+1));

    Point2d pa = tp.tp2ds[0];
    Point2d pb = tp.tp2ds[1];
    Point2d pc = tp.tp2ds[2];
    float a = (pb - pa).size();
    float b = (pc - pa).size();
    float c = (pc - pb).size();
    float p = (a + b + c) / 2.0;

    return sqrt(p * (p - a) * (p - b) * (p - c));

    //	Point2d e1 = tp.tp2ds[1]-tp.tp2ds[0];
    //	Point2d e2 = tp.tp2ds[2]-tp.tp2ds[0];
    //	return  cross(e1,e2).size()/2.0f;
}

float Mesh::computeTriangleArea(int idTri)
{
    Point3d pa = (*pts)[(*tris)[idTri].i[0]];
    Point3d pb = (*pts)[(*tris)[idTri].i[1]];
    Point3d pc = (*pts)[(*tris)[idTri].i[2]];
    float a = (pb - pa).size();
    float b = (pc - pa).size();
    float c = (pc - pb).size();
    float p = (a + b + c) / 2.0;

    return sqrt(p * (p - a) * (p - b) * (p - c));
}

StaticVector<Voxel>* Mesh::getTrianglesEdgesIds(StaticVector<StaticVector<int>*>* edgesNeighTris)
{
    StaticVector<Voxel>* out = new StaticVector<Voxel>(tris->size());
    out->resize_with(tris->size(), Voxel(-1, -1, -1));

    for(int i = 0; i < edgesNeighTris->size(); i++)
    {
        for(int j = 0; j < (*edgesNeighTris)[i]->size(); j++)
        {
            int idTri = (*(*edgesNeighTris)[i])[j];

            if((*out)[idTri].x == -1)
            {
                (*out)[idTri].x = i;
            }
            else
            {
                if((*out)[idTri].y == -1)
                {
                    (*out)[idTri].y = i;
                }
                else
                {
                    if((*out)[idTri].z == -1)
                    {
                        (*out)[idTri].z = i;
                    }
                    else
                    {
                        printf("warning ... getEdgesIdsToEachTriangle!!!\n");
                    }
                }
            }

        } // for j
    }     // for i

    // check ... each triangle has to have three edge ids
    for(int i = 0; i < tris->size(); i++)
    {
        if(((*out)[i].x == -1) || ((*out)[i].y == -1) || ((*out)[i].z == -1))
        {
            printf("warning ... getEdgesIdsToEachTriangle 1!!!\n");
        }
    }

    return out;
}

void Mesh::subdivideMeshCase1(int i, StaticVector<Pixel>* edgesi, Pixel& neptIdEdgeId,
                                 StaticVector<Mesh::triangle>* tris1)
{
    int ii[5];
    ii[0] = (*tris)[i].i[0];
    ii[1] = (*tris)[i].i[1];
    ii[2] = (*tris)[i].i[2];
    ii[3] = (*tris)[i].i[0];
    ii[4] = (*tris)[i].i[1];
    for(int k = 0; k < 3; k++)
    {
        int a = ii[k];
        int b = ii[k + 1];
        int c = ii[k + 2];
        if((((*edgesi)[neptIdEdgeId.y].x == a) && ((*edgesi)[neptIdEdgeId.y].y == b)) ||
           (((*edgesi)[neptIdEdgeId.y].y == a) && ((*edgesi)[neptIdEdgeId.y].x == b)))
        {
            Mesh::triangle t;
            t.alive = true;
            t.i[0] = a;
            t.i[1] = neptIdEdgeId.x;
            t.i[2] = c;
            tris1->push_back(t);

            t.i[0] = neptIdEdgeId.x;
            t.i[1] = b;
            t.i[2] = c;
            tris1->push_back(t);
        }
    }
}

void Mesh::subdivideMeshCase2(int i, StaticVector<Pixel>* edgesi, Pixel& neptIdEdgeId1, Pixel& neptIdEdgeId2,
                                 StaticVector<Mesh::triangle>* tris1)
{
    int ii[5];
    ii[0] = (*tris)[i].i[0];
    ii[1] = (*tris)[i].i[1];
    ii[2] = (*tris)[i].i[2];
    ii[3] = (*tris)[i].i[0];
    ii[4] = (*tris)[i].i[1];
    for(int k = 0; k < 3; k++)
    {
        int a = ii[k];
        int b = ii[k + 1];
        int c = ii[k + 2];
        if(((((*edgesi)[neptIdEdgeId1.y].x == a) && ((*edgesi)[neptIdEdgeId1.y].y == b)) ||
            (((*edgesi)[neptIdEdgeId1.y].y == a) && ((*edgesi)[neptIdEdgeId1.y].x == b))) &&
           ((((*edgesi)[neptIdEdgeId2.y].x == b) && ((*edgesi)[neptIdEdgeId2.y].y == c)) ||
            (((*edgesi)[neptIdEdgeId2.y].y == b) && ((*edgesi)[neptIdEdgeId2.y].x == c))))
        {
            Mesh::triangle t;
            t.alive = true;
            t.i[0] = a;
            t.i[1] = neptIdEdgeId1.x;
            t.i[2] = neptIdEdgeId2.x;
            tris1->push_back(t);

            t.i[0] = neptIdEdgeId1.x;
            t.i[1] = b;
            t.i[2] = neptIdEdgeId2.x;
            tris1->push_back(t);

            t.i[0] = neptIdEdgeId2.x;
            t.i[1] = c;
            t.i[2] = a;
            tris1->push_back(t);
        }
    }
}

void Mesh::subdivideMeshCase3(int i, StaticVector<Pixel>* edgesi, Pixel& neptIdEdgeId1, Pixel& neptIdEdgeId2,
                                 Pixel& neptIdEdgeId3, StaticVector<Mesh::triangle>* tris1)
{
    int a = (*tris)[i].i[0];
    int b = (*tris)[i].i[1];
    int c = (*tris)[i].i[2];
    if(((((*edgesi)[neptIdEdgeId1.y].x == a) && ((*edgesi)[neptIdEdgeId1.y].y == b)) ||
        (((*edgesi)[neptIdEdgeId1.y].y == a) && ((*edgesi)[neptIdEdgeId1.y].x == b))) &&
       ((((*edgesi)[neptIdEdgeId2.y].x == b) && ((*edgesi)[neptIdEdgeId2.y].y == c)) ||
        (((*edgesi)[neptIdEdgeId2.y].y == b) && ((*edgesi)[neptIdEdgeId2.y].x == c))) &&
       ((((*edgesi)[neptIdEdgeId3.y].x == c) && ((*edgesi)[neptIdEdgeId3.y].y == a)) ||
        (((*edgesi)[neptIdEdgeId3.y].y == c) && ((*edgesi)[neptIdEdgeId3.y].x == a))))
    {
        Mesh::triangle t;
        t.alive = true;
        t.i[0] = a;
        t.i[1] = neptIdEdgeId1.x;
        t.i[2] = neptIdEdgeId3.x;
        tris1->push_back(t);

        t.i[0] = neptIdEdgeId1.x;
        t.i[1] = b;
        t.i[2] = neptIdEdgeId2.x;
        tris1->push_back(t);

        t.i[0] = neptIdEdgeId2.x;
        t.i[1] = c;
        t.i[2] = neptIdEdgeId3.x;
        tris1->push_back(t);

        t.i[0] = neptIdEdgeId1.x;
        t.i[1] = neptIdEdgeId2.x;
        t.i[2] = neptIdEdgeId3.x;
        tris1->push_back(t);
    }
}

void Mesh::subdivideMesh(const MultiViewParams* mp, float maxTriArea, std::string tmpDir, int maxMeshPts)
{
    StaticVector<StaticVector<int>*>* trisCams = computeTrisCams(mp, tmpDir);
    StaticVector<StaticVector<int>*>* trisCams1 = subdivideMesh(mp, maxTriArea, 0.0f, true, trisCams, maxMeshPts);
    deleteArrayOfArrays<int>(&trisCams);
    deleteArrayOfArrays<int>(&trisCams1);
}

StaticVector<StaticVector<int>*>* Mesh::subdivideMesh(const MultiViewParams* mp, float maxTriArea, float maxEdgeLength,
                                                         bool useMaxTrisAreaOrAvEdgeLength,
                                                         StaticVector<StaticVector<int>*>* trisCams, int maxMeshPts)
{
    printf("SUBDIVIDING MESH\n");

    StaticVector<int>* trisCamsId = new StaticVector<int>(tris->size());
    for(int i = 0; i < tris->size(); i++)
    {
        trisCamsId->push_back(i);
    }

    int nsubd = 1000;
    while((pts->size() < maxMeshPts) && (nsubd > 10))
    {
        nsubd = subdivideMesh(mp, maxTriArea, maxEdgeLength, useMaxTrisAreaOrAvEdgeLength, trisCams, &trisCamsId);
        printf("subdivided %i\n", nsubd);
    }
    // subdivideMesh(mp, maxTriArea, trisCams, &trisCamsId);
    // subdivideMesh(mp, maxTriArea, trisCams, &trisCamsId);

    if(trisCams != nullptr)
    {
        StaticVector<StaticVector<int>*>* trisCams1 = new StaticVector<StaticVector<int>*>(tris->size());
        for(int i = 0; i < tris->size(); i++)
        {
            int tcid = (*trisCamsId)[i];
            if((*trisCams)[tcid] != nullptr)
            {
                StaticVector<int>* cams = new StaticVector<int>((*trisCams)[tcid]->size());
                for(int j = 0; j < (*trisCams)[tcid]->size(); j++)
                {
                    cams->push_back((*(*trisCams)[tcid])[j]);
                }
                trisCams1->push_back(cams);
            }
            else
            {
                trisCams1->push_back(nullptr);
            }
        }
        delete trisCamsId;

        return trisCams1;
    }

    return nullptr;
}

void Mesh::subdivideMeshMaxEdgeLengthUpdatePtsCams(const MultiViewParams* mp, float maxEdgeLength,
                                                      StaticVector<StaticVector<int>*>* ptsCams, int maxMeshPts)
{
    printf("SUBDIVIDING MESH\n");

    StaticVector<int>* trisCamsId = new StaticVector<int>(tris->size());
    for(int i = 0; i < tris->size(); i++)
    {
        trisCamsId->push_back(i);
    }

    int oldNPts = pts->size();
    int oldNTris = tris->size();
    StaticVector<triangle>* oldTris = new StaticVector<triangle>(tris->size());
    oldTris->push_back_arr(tris);

    int nsubd = 1000;
    while((pts->size() < maxMeshPts) && (nsubd > 10))
    {
        nsubd = subdivideMesh(mp, 0.0f, maxEdgeLength, false, nullptr, &trisCamsId);
        printf("subdivided %i\n", nsubd);
    }
    // subdivideMesh(mp, maxTriArea, trisCams, &trisCamsId);
    // subdivideMesh(mp, maxTriArea, trisCams, &trisCamsId);

    if(pts->size() - oldNPts > 0)
    {
        StaticVector<int>* newPtsOldTriId = new StaticVector<int>(pts->size() - oldNPts);
        newPtsOldTriId->resize_with(pts->size() - oldNPts, -1);
        for(int i = oldNTris; i < tris->size(); i++)
        {
            int tcid = (*trisCamsId)[i];
            while(tcid > oldNTris)
            {
                tcid = (*trisCamsId)[tcid];
            }
            for(int k = 0; k < 3; k++)
            {
                if((*tris)[i].i[k] >= oldNPts)
                {
                    (*newPtsOldTriId)[(*tris)[i].i[k] - oldNPts] = tcid;
                }
            }
        }

        ptsCams->resizeAdd(newPtsOldTriId->size());
        for(int i = 0; i < newPtsOldTriId->size(); i++)
        {
            StaticVector<int>* cams = nullptr;
            int idTri = (*newPtsOldTriId)[i];
            if(idTri > -1)
            {
                if(((*oldTris)[idTri].i[0] >= oldNPts) || ((*oldTris)[idTri].i[1] >= oldNPts) ||
                   ((*oldTris)[idTri].i[2] >= oldNPts))
                {
                    throw std::runtime_error("subdivideMeshMaxEdgeLengthUpdatePtsCams: out of range.");
                }

                int maxcams = sizeOfStaticVector<int>((*ptsCams)[(*oldTris)[idTri].i[0]]) +
                              sizeOfStaticVector<int>((*ptsCams)[(*oldTris)[idTri].i[1]]) +
                              sizeOfStaticVector<int>((*ptsCams)[(*oldTris)[idTri].i[2]]);
                cams = new StaticVector<int>(maxcams);
                for(int k = 0; k < 3; k++)
                {
                    for(int j = 0; j < sizeOfStaticVector<int>((*ptsCams)[(*oldTris)[idTri].i[k]]); j++)
                    {
                        cams->push_back_distinct((*(*ptsCams)[(*oldTris)[idTri].i[k]])[j]);
                    }
                }
                cams->shrink_to_fit();
            }
            ptsCams->push_back(cams);
        }

        if(ptsCams->size() != pts->size())
        {
            throw std::runtime_error("subdivideMeshMaxEdgeLengthUpdatePtsCams: different size!");
        }

        delete newPtsOldTriId;
    }

    delete oldTris;
    delete trisCamsId;
}

int Mesh::subdivideMesh(const MultiViewParams* mp, float maxTriArea, float maxEdgeLength,
                           bool useMaxTrisAreaOrAvEdgeLength, StaticVector<StaticVector<int>*>* trisCams,
                           StaticVector<int>** trisCamsId)
{

    StaticVector<StaticVector<int>*>* edgesNeighTris;
    StaticVector<Pixel>* edgesPointsPairs;
    getNotOrientedEdges(&edgesNeighTris, &edgesPointsPairs);
    StaticVector<Voxel>* trisEdges = getTrianglesEdgesIds(edgesNeighTris);

    // which triangles should be subdivided
    int nTrisToSubdivide = 0;
    StaticVectorBool* trisToSubdivide = new StaticVectorBool(tris->size());
    for(int i = 0; i < tris->size(); i++)
    {
        int tcid = (*(*trisCamsId))[i];
        bool subdivide = false;

        if(useMaxTrisAreaOrAvEdgeLength)
        {
            for(int j = 0; j < sizeOfStaticVector<int>((*trisCams)[tcid]); j++)
            {
                int rc = (*(*trisCams)[tcid])[j];
                int w = mp->mip->getWidth(rc);
                int h = mp->mip->getHeight(rc);
                triangle_proj tp = getTriangleProjection(i, mp, rc, w, h);
                if(computeTriangleProjectionArea(tp) > maxTriArea)
                {
                    subdivide = true;
                }
            }
        }
        else
        {
            if(computeTriangleMaxEdgeLength(i) > maxEdgeLength)
            {
                subdivide = true;
            }
        }

        (*trisToSubdivide)[i] = subdivide;
        nTrisToSubdivide += static_cast<int>(subdivide);
    }

    // which edges are going to be subdivided
    StaticVector<int>* edgesToSubdivide = new StaticVector<int>(edgesNeighTris->size());
    for(int i = 0; i < edgesNeighTris->size(); i++)
    {
        bool hasNeigTriToSubdivide = false;
        for(int j = 0; j < (*edgesNeighTris)[i]->size(); j++)
        {
            int idTri = (*(*edgesNeighTris)[i])[j];
            if((*trisToSubdivide)[idTri])
            {
                hasNeigTriToSubdivide = true;
            }
        }
        edgesToSubdivide->push_back((int)(hasNeigTriToSubdivide)-1);
    }

    // assing id-s
    int id = pts->size();
    for(int i = 0; i < edgesToSubdivide->size(); i++)
    {
        if((*edgesToSubdivide)[i] > -1)
        {
            (*edgesToSubdivide)[i] = id;
            id++;
        }
    }
    int nEdgesToSubdivide = id - pts->size();

    // copy old pts
    StaticVector<Point3d>* pts1 = new StaticVector<Point3d>(pts->size() + nEdgesToSubdivide);
    for(int i = 0; i < pts->size(); i++)
    {
        pts1->push_back((*pts)[i]);
    }

    // add new pts ... middle of edge
    for(int i = 0; i < edgesToSubdivide->size(); i++)
    {
        if((*edgesToSubdivide)[i] > -1)
        {
            Point3d p = ((*pts)[(*edgesPointsPairs)[i].x] + (*pts)[(*edgesPointsPairs)[i].y]) / 2.0f;
            pts1->push_back(p);
        }
    }

    // there might be needed to subdivide more triangles ... find them
    nTrisToSubdivide = 0;
    for(int i = 0; i < tris->size(); i++)
    {
        bool subdivide =
            (((*edgesToSubdivide)[(*trisEdges)[i].x] > -1) || ((*edgesToSubdivide)[(*trisEdges)[i].y] > -1) ||
             ((*edgesToSubdivide)[(*trisEdges)[i].z] > -1));
        (*trisToSubdivide)[i] = subdivide;
        nTrisToSubdivide += static_cast<int>(subdivide);
    }

    printf("number of triangles to subdivide %i\n", nTrisToSubdivide);
    printf("number of pts to add %i\n", nEdgesToSubdivide);

    StaticVector<int>* trisCamsId1 = new StaticVector<int>(tris->size() - nTrisToSubdivide + 4 * nTrisToSubdivide);
    StaticVector<Mesh::triangle>* tris1 =
        new StaticVector<Mesh::triangle>(tris->size() - nTrisToSubdivide + 4 * nTrisToSubdivide);
    for(int i = 0; i < tris->size(); i++)
    {
        if((*trisToSubdivide)[i])
        {
            Pixel newPtsIds[3];
            newPtsIds[0].x = (*edgesToSubdivide)[(*trisEdges)[i].x]; // new pt id
            newPtsIds[0].y = (*trisEdges)[i].x;                      // edge id
            newPtsIds[1].x = (*edgesToSubdivide)[(*trisEdges)[i].y];
            newPtsIds[1].y = (*trisEdges)[i].y;
            newPtsIds[2].x = (*edgesToSubdivide)[(*trisEdges)[i].z];
            newPtsIds[2].y = (*trisEdges)[i].z;

            qsort(&newPtsIds[0], 3, sizeof(Pixel), qSortComparePixelByXDesc);

            int n = 0;
            while((n < 3) && (newPtsIds[n].x > -1))
            {
                n++;
            }

            if(n == 0)
            {
                printf("warning subdivideMesh 1 !!!");
            }

            if(n == 1)
            {
                subdivideMeshCase1(i, edgesPointsPairs, newPtsIds[0], tris1);

                trisCamsId1->push_back((*(*trisCamsId))[i]);
                trisCamsId1->push_back((*(*trisCamsId))[i]);
            }

            if(n == 2)
            {
                subdivideMeshCase2(i, edgesPointsPairs, newPtsIds[0], newPtsIds[1], tris1);
                subdivideMeshCase2(i, edgesPointsPairs, newPtsIds[1], newPtsIds[0], tris1);

                trisCamsId1->push_back((*(*trisCamsId))[i]);
                trisCamsId1->push_back((*(*trisCamsId))[i]);
                trisCamsId1->push_back((*(*trisCamsId))[i]);
            }

            if(n == 3)
            {
                subdivideMeshCase3(i, edgesPointsPairs, newPtsIds[0], newPtsIds[1], newPtsIds[2], tris1);
                subdivideMeshCase3(i, edgesPointsPairs, newPtsIds[0], newPtsIds[2], newPtsIds[1], tris1);
                subdivideMeshCase3(i, edgesPointsPairs, newPtsIds[1], newPtsIds[0], newPtsIds[2], tris1);
                subdivideMeshCase3(i, edgesPointsPairs, newPtsIds[1], newPtsIds[2], newPtsIds[0], tris1);
                subdivideMeshCase3(i, edgesPointsPairs, newPtsIds[2], newPtsIds[0], newPtsIds[1], tris1);
                subdivideMeshCase3(i, edgesPointsPairs, newPtsIds[2], newPtsIds[1], newPtsIds[0], tris1);

                trisCamsId1->push_back((*(*trisCamsId))[i]);
                trisCamsId1->push_back((*(*trisCamsId))[i]);
                trisCamsId1->push_back((*(*trisCamsId))[i]);
                trisCamsId1->push_back((*(*trisCamsId))[i]);
            }
        }
        else
        {
            tris1->push_back((*tris)[i]);
            trisCamsId1->push_back((*(*trisCamsId))[i]);
        }
    }

    delete pts;
    delete tris;
    pts = pts1;
    tris = tris1;

    delete(*trisCamsId);
    (*trisCamsId) = trisCamsId1;

    deleteArrayOfArrays<int>(&edgesNeighTris);
    delete edgesPointsPairs;
    delete trisEdges;
    delete trisToSubdivide;
    delete edgesToSubdivide;

    return nTrisToSubdivide;
}

float Mesh::computeAverageEdgeLength() const
{
    /*
    StaticVector<StaticVector<int>*> *edgesNeighTris;
    StaticVector<pixel> *edgesPointsPairs;
    getNotOrientedEdges(&edgesNeighTris, &edgesPointsPairs);

    float s=0.0f;
    float n=0.0f;
    for (int i=0;i<edgesPointsPairs->size();i++) {
            s += ((*pts)[(*edgesPointsPairs)[i].x]-(*pts)[(*edgesPointsPairs)[i].y]).size();
            n += 1.0f;
    };

    deleteArrayOfArrays<int>(&edgesNeighTris);
    delete edgesPointsPairs;

    if (n==0.0f) {
            return 0.0f;
    }else{
            return (s/n);
    };
    */

    float s = 0.0f;
    float n = 0.0f;
    for(int i = 0; i < tris->size(); i++)
    {
        s += computeTriangleMaxEdgeLength(i);
        n += 1.0f;
    }
    if(n == 0.0f)
    {
        return 0.0f;
    }

    return (s / n);
}

void Mesh::letJustTringlesIdsInMesh(StaticVector<int>* trisIdsToStay)
{
    // printf("letJustTringlesIdsInMesh %i\n",trisIdsToStay->size());

    StaticVector<Mesh::triangle>* trisTmp = new StaticVector<Mesh::triangle>(trisIdsToStay->size());
    for(int i = 0; i < trisIdsToStay->size(); i++)
    {
        trisTmp->push_back((*tris)[(*trisIdsToStay)[i]]);
    }

    delete tris;
    tris = trisTmp;
}

StaticVector<StaticVector<int>*>* Mesh::computeTrisCams(const MultiViewParams* mp, std::string tmpDir)
{
    if(mp->verbose)
        printf("computing tris cams\n");

    StaticVector<int>* ntrisCams = new StaticVector<int>(tris->size());
    ntrisCams->resize_with(tris->size(), 0);

    long t1 = initEstimate();
    for(int rc = 0; rc < mp->ncams; rc++)
    {
        std::string visTrisFileName = tmpDir + "visTris" + std::to_string(mp->mip->getViewId(rc)) + ".bin";
        StaticVector<int>* visTris = loadArrayFromFile<int>(visTrisFileName);
        if(visTris != nullptr)
        {
            for(int i = 0; i < visTris->size(); i++)
            {
                int idTri = (*visTris)[i];
                (*ntrisCams)[idTri]++;
            }
            delete visTris;
        }
        printfEstimate(rc, mp->ncams, t1);
    }
    finishEstimate();

    StaticVector<StaticVector<int>*>* trisCams = new StaticVector<StaticVector<int>*>(tris->size());
    for(int i = 0; i < tris->size(); i++)
    {
        StaticVector<int>* cams = nullptr;
        if((*ntrisCams)[i] > 0)
        {
            cams = new StaticVector<int>((*ntrisCams)[i]);
        }
        trisCams->push_back(cams);
    }

    t1 = initEstimate();
    for(int rc = 0; rc < mp->ncams; rc++)
    {
        std::string visTrisFileName = tmpDir + "visTris" + std::to_string(mp->mip->getViewId(rc)) + ".bin";
        StaticVector<int>* visTris = loadArrayFromFile<int>(visTrisFileName);
        if(visTris != nullptr)
        {
            for(int i = 0; i < visTris->size(); i++)
            {
                int idTri = (*visTris)[i];
                (*trisCams)[idTri]->push_back(rc);
            }
            delete visTris;
        }
        printfEstimate(rc, mp->ncams, t1);
    }
    finishEstimate();

    delete ntrisCams;

    if(mp->verbose)
        printf("done\n");

    return trisCams;
}

StaticVector<StaticVector<int>*>* Mesh::computeTrisCamsFromPtsCams(StaticVector<StaticVector<int>*>* ptsCams) const
{
    //std::cout << "computeTrisCamsFromPtsCams" << std::endl;
    // TODO: try intersection
    StaticVector<StaticVector<int>*>* trisCams = new StaticVector<StaticVector<int>*>(tris->size());
    for(int idTri = 0; idTri < tris->size(); idTri++)
    {
        int maxcams = sizeOfStaticVector<int>((*ptsCams)[(*tris)[idTri].i[0]]) +
                      sizeOfStaticVector<int>((*ptsCams)[(*tris)[idTri].i[1]]) +
                      sizeOfStaticVector<int>((*ptsCams)[(*tris)[idTri].i[2]]);
        StaticVector<int>* cams = new StaticVector<int>(maxcams);
        for(int k = 0; k < 3; k++)
        {
            for(int i = 0; i < sizeOfStaticVector<int>((*ptsCams)[(*tris)[idTri].i[k]]); i++)
            {
                cams->push_back_distinct((*(*ptsCams)[(*tris)[idTri].i[k]])[i]);
            }
        }
        trisCams->push_back(cams);
    }

    //std::cout << "computeTrisCamsFromPtsCams end" << std::endl;
    return trisCams;
}

void Mesh::initFromDepthMap(const MultiViewParams* mp, StaticVector<float>* depthMap, int rc, int scale, float alpha)
{
    initFromDepthMap(mp, &(*depthMap)[0], rc, scale, 1, alpha);
}

void Mesh::initFromDepthMap(const MultiViewParams* mp, float* depthMap, int rc, int scale, int step, float alpha)
{
    initFromDepthMap(1, mp, depthMap, rc, scale, step, alpha);
}

void Mesh::initFromDepthMap(int stepDetail, const MultiViewParams* mp, float* depthMap, int rc, int scale, int step,
                               float alpha)
{
    int w = mp->mip->getWidth(rc) / (scale * step);
    int h = mp->mip->getHeight(rc) / (scale * step);

    pts = new StaticVector<Point3d>(w * h);
    StaticVectorBool* usedMap = new StaticVectorBool(w * h);
    for(int i = 0; i < w * h; i++)
    {
        int x = i / h;
        int y = i % h;
        float depth = depthMap[i];
        if(depth > 0.0f)
        {
            Point3d p = mp->CArr[rc] +
                        (mp->iCamArr[rc] * Point2d((float)x * (float)(scale * step), (float)y * (float)(scale * step)))
                                .normalize() *
                            depth;
            pts->push_back(p);
            usedMap->push_back(true);
        }
        else
        {
            pts->push_back(Point3d(0.0f, 0.0f, 0.0f));
            usedMap->push_back(false);
        }
    }

    tris = new StaticVector<Mesh::triangle>(w * h * 2);
    for(int x = 0; x < w - 1 - stepDetail; x += stepDetail)
    {
        for(int y = 0; y < h - 1 - stepDetail; y += stepDetail)
        {
            Point3d p1 = (*pts)[x * h + y];
            Point3d p2 = (*pts)[(x + stepDetail) * h + y];
            Point3d p3 = (*pts)[(x + stepDetail) * h + y + stepDetail];
            Point3d p4 = (*pts)[x * h + y + stepDetail];

            if((*usedMap)[x * h + y] && (*usedMap)[(x + stepDetail) * h + y] &&
               (*usedMap)[(x + stepDetail) * h + y + stepDetail] && (*usedMap)[x * h + y + stepDetail])
            {
                float d = mp->getCamPixelSize(p1, rc, alpha);
                if(((p1 - p2).size() < d) && ((p1 - p3).size() < d) && ((p1 - p4).size() < d) &&
                   ((p2 - p3).size() < d) && ((p3 - p4).size() < d))
                {
                    Mesh::triangle t;
                    t.alive = true;
                    t.i[2] = x * h + y;
                    t.i[1] = (x + stepDetail) * h + y;
                    t.i[0] = x * h + y + stepDetail;
                    tris->push_back(t);

                    t.alive = true;
                    t.i[2] = (x + stepDetail) * h + y;
                    t.i[1] = (x + stepDetail) * h + y + stepDetail;
                    t.i[0] = x * h + y + stepDetail;
                    tris->push_back(t);
                }
            }
        }
    }

    delete usedMap;

    removeFreePointsFromMesh(nullptr);
}

void Mesh::removeTrianglesInHexahedrons(StaticVector<Point3d>* hexahsToExcludeFromResultingMesh)
{
    if(hexahsToExcludeFromResultingMesh != nullptr)
    {
        printf("removeTrianglesInHexahedrons %i %i \n", tris->size(),
               (int)(hexahsToExcludeFromResultingMesh->size() / 8));
        StaticVector<int>* trisIdsToStay = new StaticVector<int>(tris->size());

        long t1 = initEstimate();
        for(int i = 0; i < tris->size(); i++)
        {
            int nin = 0;
            for(int k = 0; k < 3; k++)
            {
                Point3d p = (*pts)[(*tris)[i].i[k]];
                bool isThere = false;
                for(int j = 0; j < (int)(hexahsToExcludeFromResultingMesh->size() / 8); j++)
                {
                    if(isPointInHexahedron(p, &(*hexahsToExcludeFromResultingMesh)[j * 8]))
                    {
                        isThere = true;
                    }
                }
                if(isThere)
                {
                    nin++;
                }
            }
            if(nin < 3)
            {
                trisIdsToStay->push_back(i);
            }
            printfEstimate(i, tris->size(), t1);
        }
        finishEstimate();

        letJustTringlesIdsInMesh(trisIdsToStay);

        delete trisIdsToStay;
    }
}

void Mesh::removeTrianglesOutsideHexahedron(Point3d* hexah)
{
    printf("removeTrianglesOutsideHexahedrons %i \n", tris->size());
    StaticVector<int>* trisIdsToStay = new StaticVector<int>(tris->size());

    long t1 = initEstimate();
    for(int i = 0; i < tris->size(); i++)
    {
        int nout = 0;
        for(int k = 0; k < 3; k++)
        {
            Point3d p = (*pts)[(*tris)[i].i[k]];
            bool isThere = false;
            if(!isPointInHexahedron(p, hexah))
            {
                isThere = true;
            }
            if(isThere)
            {
                nout++;
            }
        }
        if(nout < 1)
        {
            trisIdsToStay->push_back(i);
        }
        printfEstimate(i, tris->size(), t1);
    }
    finishEstimate();

    letJustTringlesIdsInMesh(trisIdsToStay);

    delete trisIdsToStay;
}

void Mesh::filterLargeEdgeTriangles(float maxEdgelengthThr)
{
    float averageEdgeLength = computeAverageEdgeLength();
    float avelthr = maxEdgelengthThr;

    StaticVector<int>* trisIdsToStay = new StaticVector<int>(tris->size());
    for(int i = 0; i < tris->size(); i++)
    {
        float triMaxEdgelength = computeTriangleMaxEdgeLength(i);
        if(triMaxEdgelength < averageEdgeLength * avelthr)
        {
            trisIdsToStay->push_back(i);
        }
    }
    letJustTringlesIdsInMesh(trisIdsToStay);
    delete trisIdsToStay;
}

void Mesh::invertTriangleOrientations()
{
    std::cout << "invertTriangleOrientations" << std::endl;
    for(int i = 0; i < tris->size(); ++i)
    {
        Mesh::triangle& t = (*tris)[i];
        std::swap(t.i[1], t.i[2]);
    }
}

void Mesh::changeTriPtId(int triId, int oldPtId, int newPtId)
{
    for(int k = 0; k < 3; k++)
    {
        if(oldPtId == (*tris)[triId].i[k])
        {
            (*tris)[triId].i[k] = newPtId;
        }
    }
}

int Mesh::getTriPtIndex(int triId, int ptId, bool failIfDoesNotExists)
{
    for(int k = 0; k < 3; k++)
    {
        if(ptId == (*tris)[triId].i[k])
        {
            return k;
        }
    }

    if(failIfDoesNotExists)
    {
        throw std::runtime_error("Mesh::getTriPtIndex: ptId does not exist: " + std::to_string(ptId));
    }
    return -1;
}

Pixel Mesh::getTriOtherPtsIds(int triId, int _ptId)
{
    int others[3];
    int nothers = 0;
    for(int k = 0; k < 3; k++)
    {
        if(_ptId != (*tris)[triId].i[k])
        {
            others[nothers] = (*tris)[triId].i[k];
            nothers++;
        }
    }

    if(nothers != 2)
    {
        throw std::runtime_error("Mesh::getTriOtherPtsIds: pt X neighbouring tringle without pt X");
    }

    return Pixel(others[0], others[1]);
}
bool Mesh::areTwoTrisSameOriented(int triId1, int triId2, int edgePtId1, int edgePtId2)
{
    int t1ep1Index = getTriPtIndex(triId1, edgePtId1, true);
    int t1ep2Index = getTriPtIndex(triId1, edgePtId2, true);
    int t2ep1Index = getTriPtIndex(triId2, edgePtId1, true);
    int t2ep2Index = getTriPtIndex(triId2, edgePtId2, true);
    int t1Orientation = (t1ep2Index + (3 - t1ep1Index)) % 3; // can be 1 or 2;
    int t2Orientation = (t2ep2Index + (3 - t2ep1Index)) % 3; // can be 1 or 2;

    return (t1Orientation != t2Orientation);
}

bool Mesh::isTriangleAngleAtVetexObtuse(int vertexIdInTriangle, int triId)
{
    Point3d A = (*pts)[(*tris)[triId].i[(vertexIdInTriangle + 0) % 3]];
    Point3d B = (*pts)[(*tris)[triId].i[(vertexIdInTriangle + 1) % 3]];
    Point3d C = (*pts)[(*tris)[triId].i[(vertexIdInTriangle + 2) % 3]];
    return dot(B - A, C - A) < 0.0f;
}

bool Mesh::isTriangleObtuse(int triId)
{
    return (isTriangleAngleAtVetexObtuse(0, triId)) || (isTriangleAngleAtVetexObtuse(1, triId)) ||
           (isTriangleAngleAtVetexObtuse(2, triId));
}

StaticVector<int>* Mesh::getLargestConnectedComponentTrisIds(const MultiViewParams& mp)
{
    StaticVector<StaticVector<int>*>* ptsNeighPtsOrdered = getPtsNeighPtsOrdered();

    StaticVector<int>* colors = new StaticVector<int>(pts->size());
    colors->resize_with(pts->size(), -1);
    StaticVector<int>* buff = new StaticVector<int>(pts->size());
    int col = 0;
    int maxNptsOfCol = -1;
    int bestCol = -1;
    for(int i = 0; i < pts->size(); i++)
    {
        if((*colors)[i] == -1)
        {
            buff->resize(0);
            buff->push_back(i);
            int nptsOfCol = 0;
            while(buff->size() > 0)
            {
                int ptid = buff->pop();
                if((*colors)[ptid] == -1)
                {
                    (*colors)[ptid] = col;
                    nptsOfCol++;
                }
                else
                {
                    if((*colors)[ptid] != col)
                    {
                        if(mp.verbose)
                            printf("WARNING should not happen!\n");
                    }
                }
                for(int j = 0; j < sizeOfStaticVector<int>((*ptsNeighPtsOrdered)[ptid]); j++)
                {
                    int nptid = (*(*ptsNeighPtsOrdered)[ptid])[j];
                    if((nptid > -1) && ((*colors)[nptid] == -1))
                    {
                        if(buff->size() >= buff->capacity())
                        {
                            if(mp.verbose)
                                printf("WARNING should not happen but no problem!\n");
                            buff->resizeAdd(pts->size());
                        }
                        buff->push_back(nptid);
                    }
                }
            }

            if(maxNptsOfCol < nptsOfCol)
            {
                maxNptsOfCol = nptsOfCol;
                bestCol = col;
            }
            col++;
        }
    }

    StaticVector<int>* out = new StaticVector<int>(tris->size());
    for(int i = 0; i < tris->size(); i++)
    {
        if(((*tris)[i].alive) && ((*colors)[(*tris)[i].i[0]] == bestCol) && ((*colors)[(*tris)[i].i[1]] == bestCol) &&
           ((*colors)[(*tris)[i].i[2]] == bestCol))
        {
            out->push_back(i);
        }
    }

    delete colors;
    delete buff;
    deleteArrayOfArrays<int>(&ptsNeighPtsOrdered);

    return out;
}

bool Mesh::loadFromObjAscii(int& nmtls, StaticVector<int>** trisMtlIds, StaticVector<Point3d>** normals,
                               StaticVector<Voxel>** trisNormalsIds, StaticVector<Point2d>** uvCoords,
                               StaticVector<Voxel>** trisUvIds, std::string objAsciiFileName)
{
    std::cout << "Loading mesh from obj file: " << objAsciiFileName << std::endl;
    // read number of points, triangles, uvcoords
    int npts = 0;
    int ntris = 0;
    int nuvs = 0;
    int nnorms = 0;
    int nlines = 0;

    {
        std::ifstream in(objAsciiFileName.c_str());
        std::string line;
        while(getline(in, line))
        {
            if((line[0] == 'v') && (line[1] == ' '))
            {
                npts += 1;
            }
            if((line[0] == 'v') && (line[1] == 'n') && (line[2] == ' '))
            {
                nnorms += 1;
            }
            if((line[0] == 'v') && (line[1] == 't') && (line[2] == ' '))
            {
                nuvs += 1;
            }
            if((line[0] == 'f') && (line[1] == ' '))
            {
                ntris += 1;
            }
            nlines++;
        }
        in.close();
    }

    printf("vertices %i\n", npts);
    printf("normals %i\n", nnorms);
    printf("uv coordinates %i\n", nuvs);
    printf("triangles %i\n", ntris);

    pts = new StaticVector<Point3d>(npts);
    tris = new StaticVector<Mesh::triangle>(ntris);
    *uvCoords = new StaticVector<Point2d>(nuvs);
    *trisUvIds = new StaticVector<Voxel>(ntris);
    *normals = new StaticVector<Point3d>(nnorms);
    *trisNormalsIds = new StaticVector<Voxel>(ntris);
    *trisMtlIds = new StaticVector<int>(ntris);

    std::map<std::string, int> materialCache;

    {
        int mtlId = -1;
        std::ifstream in(objAsciiFileName.c_str());
        std::string line;

        long t1 = initEstimate();
        int idline = 0;
        while(getline(in, line))
        {
            if(findNSubstrsInString(line, "usemtl") == 1)
            {
                char buff[5000];
                sscanf(line.c_str(), "usemtl %s", buff);
                auto it = materialCache.find(buff);
                if(it == materialCache.end())
                    materialCache.emplace(buff, ++mtlId); // new material
                else
                    mtlId = it->second;                   // already known material
            }

            if((line[0] == 'v') && (line[1] == ' '))
            {
                Point3d pt;
                sscanf(line.c_str(), "v %lf %lf %lf", &pt.x, &pt.y, &pt.z);
                pts->push_back(pt);
            }

            if((line[0] == 'v') && (line[1] == 'n') && (line[2] == ' '))
            {
                Point3d pt;
                sscanf(line.c_str(), "vn %lf %lf %lf", &pt.x, &pt.y, &pt.z);
                // printf("%f %f %f\n", pt.x, pt.y, pt.z);
                (*normals)->push_back(pt);
            }

            if((line[0] == 'v') && (line[1] == 't') && (line[2] == ' '))
            {
                Point2d pt;
                sscanf(line.c_str(), "vt %lf %lf", &pt.x, &pt.y);
                (*uvCoords)->push_back(pt);
            }

            if((line[0] == 'f') && (line[1] == ' '))
            {
                int n1 = findNSubstrsInString(line, "/");
                int n2 = findNSubstrsInString(line, "//");
                Voxel v1, v2, v3;
                bool ok = false;
                bool okn = false;
                bool okuv = false;
                if(n2 == 0)
                {
                    if(n1 == 0)
                    {
                        sscanf(line.c_str(), "f %i %i %i", &v1.x, &v1.y, &v1.z);
                        ok = true;
                    }
                    if(n1 == 3)
                    {
                        sscanf(line.c_str(), "f %i/%i %i/%i %i/%i", &v1.x, &v2.x, &v1.y, &v2.y, &v1.z, &v2.z);
                        ok = true;
                        okuv = true;
                    }
                    if(n1 == 6)
                    {
                        sscanf(line.c_str(), "f %i/%i/%i %i/%i/%i %i/%i/%i", &v1.x, &v2.x, &v3.x, &v1.y, &v2.y, &v3.y,
                               &v1.z, &v2.z, &v3.z);
                        ok = true;
                        okuv = true;
                        okn = true;
                    }
                }
                else
                {
                    if(n2 == 3)
                    {
                        sscanf(line.c_str(), "f %i//%i %i//%i %i//%i", &v1.x, &v3.x, &v1.y, &v3.y, &v1.z, &v3.z);
                        ok = true;
                        okn = true;
                    }
                }

                if(!ok)
                {
                    throw std::runtime_error("Mesh: Error occured while reading obj file: " + objAsciiFileName);
                }

                triangle t;
                t.i[0] = v1.x - 1;
                t.i[1] = v1.y - 1;
                t.i[2] = v1.z - 1;
                t.alive = true;
                tris->push_back(t);
                (*trisMtlIds)->push_back(mtlId);

                if(okuv)
                {
                    (*trisUvIds)->push_back(v2 - Voxel(1, 1, 1));
                }

                if(okn)
                {
                    (*trisNormalsIds)->push_back(v3 - Voxel(1, 1, 1));
                }
            }

            printfEstimate(idline, nlines, t1);
            idline++;
        }
        finishEstimate();

        in.close();
        nmtls = materialCache.size();
    }
    return npts != 0 && ntris != 0;
}

bool Mesh::getEdgeNeighTrisInterval(Pixel& itr, Pixel edge, StaticVector<Voxel>* edgesXStat,
                                       StaticVector<Voxel>* edgesXYStat)
{
    int ptId1 = std::max(edge.x, edge.y);
    int ptId2 = std::min(edge.x, edge.y);
    itr = Pixel(-1, -1);

    int i1 = indexOfSortedVoxelArrByX(ptId1, edgesXStat, 0, edgesXStat->size() - 1);
    if(i1 > -1)
    {
        int i2 = indexOfSortedVoxelArrByX(ptId2, edgesXYStat, (*edgesXStat)[i1].y, (*edgesXStat)[i1].z);
        if(i2 > -1)
        {
            itr = Pixel((*edgesXYStat)[i2].y, (*edgesXYStat)[i2].z);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

} // namespace aliceVision
