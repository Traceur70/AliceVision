// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "AlembicImporter.hpp"
#include "sfm_data.hpp"
#include "sfm_data_io.hpp"

#include "testing/testing.h"
#include "dependencies/stlplus3/filesystemSimplified/file_system.hpp"


using namespace aliceVision;
using namespace aliceVision::sfm;


// Create a SfM scene with desired count of views & poses & intrinsic (shared or not)
// Add a 3D point with observation in 2 view (just in order to have non empty data)
SfM_Data create_test_scene(IndexT singleViewsCount,
                           IndexT pointCount,
                           IndexT rigCount,
                           IndexT subPoseCount,
                           bool bSharedIntrinsic)
{
  SfM_Data sfm_data;
  sfm_data.s_root_path = "./";

  std::srand(time(nullptr));

  for(IndexT i = 0; i < singleViewsCount; ++i)
  {
    // Add views
    std::ostringstream os;
    os << "dataset/" << i << ".jpg";
    const IndexT id_view = i, id_pose = i;
    const IndexT id_intrinsic = bSharedIntrinsic ? 0 : i; //(shared or not intrinsics)

    std::shared_ptr<View> view = std::make_shared<View>(os.str(),id_view, id_intrinsic, id_pose, 1000, 1000);
    sfm_data.views[id_view] = view;

    // Add poses
    const Mat3 r = Mat3::Random();
    const Vec3 c = Vec3::Random();
    sfm_data.setPose(*view, geometry::Pose3(r, c));

    // Add intrinsics
    if (bSharedIntrinsic)
    {
      if (i == 0)
        sfm_data.intrinsics[0] = std::make_shared<camera::Pinhole>(1000, 1000, 36.0, std::rand()%10000, std::rand()%10000);
    }
    else
    {
      sfm_data.intrinsics[i] = std::make_shared<camera::Pinhole>(1000, 1000, 36.0, std::rand()%10000, std::rand()%10000);
    }
  }


  std::size_t nbIntrinsics = (bSharedIntrinsic ? 1 : singleViewsCount);
  std::size_t nbPoses = singleViewsCount;
  std::size_t nbViews = singleViewsCount;
  const std::size_t nbRigPoses = 5;

  for(IndexT rigId = 0; rigId < rigCount; ++rigId)
  {
    sfm_data.getRigs().emplace(rigId, Rig(subPoseCount));
    Rig& rig = sfm_data.getRigs().at(rigId);

    for(IndexT subPoseId = 0; subPoseId < subPoseCount; ++subPoseId)
    {
      rig.setSubPose(subPoseId, RigSubPose());
      sfm_data.intrinsics[nbIntrinsics + subPoseId] = std::make_shared<camera::Pinhole>(1000, 1000, 36.0, std::rand()%10000, std::rand()%10000);

      for(std::size_t pose = 0; pose < nbRigPoses; ++pose)
      {
        std::ostringstream os;
        os << "dataset/rig_" << rigId << "_"  <<  subPoseId << "_" << pose << ".jpg";

        std::shared_ptr<View> view = std::make_shared<View>(os.str(),
                                                            nbViews,
                                                            nbIntrinsics + subPoseId,
                                                            nbPoses + pose,
                                                            1000,
                                                            1000,
                                                            rigId,
                                                            subPoseId);

        if((pose == 0) || (subPoseId == 0))
        {
          const Mat3 r = Mat3::Random();
          const Vec3 c = Vec3::Random();
          sfm_data.setPose(*view, geometry::Pose3(r, c));
        }

        sfm_data.views[nbViews] = view;
        ++nbViews;
      }
    }
    nbPoses += nbRigPoses;
    nbIntrinsics += subPoseCount;
  }

  // Fill with not meaningful tracks
  for(IndexT i = 0; i < pointCount; ++i)
  {
    // Add structure
    Observations observations;
    observations[0] = Observation( Vec2(std::rand() % 10000, std::rand() % 10000), 0);
    observations[1] = Observation( Vec2(std::rand() % 10000, std::rand() % 10000), 1);
    sfm_data.structure[i].observations = observations;
    sfm_data.structure[i].X = Vec3(std::rand() % 10000, std::rand() % 10000, std::rand() % 10000);
    sfm_data.structure[i].rgb = image::RGBColor((std::rand() % 1000) / 1000.0, (std::rand() % 1000) / 1000.0, (std::rand() % 1000) / 1000.0);
    sfm_data.structure[i].descType = features::EImageDescriberType::SIFT;

    // Add control points    
  }

  return sfm_data;
}

//-----------------
// Test summary:
//-----------------
// - Create a random scene (.json)
// - Export to Alembic
// - Import to Alembic
// - Import to .json
//-----------------
TEST(AlembicImporter, importExport) {

    int flags = ALL;

    // Create a random scene
    const SfM_Data sfm_data = create_test_scene(5, 50, 2, 3, true);
    

    /*********************************************************************/
    /*****************              JSON -> JSON           ***************/
    /*********************************************************************/

    // Export as JSON
    const std::string jsonFile = "jsonToJson.json";
    {
        EXPECT_TRUE(Save(
        sfm_data,
        jsonFile.c_str(),
        ESfM_Data(flags)));
    }

    // Reload
    SfM_Data sfmJsonToJson;
    {
        EXPECT_TRUE(Load(sfmJsonToJson, jsonFile, ESfM_Data(flags)));
        EXPECT_TRUE(sfm_data == sfmJsonToJson);
    }

    // Export as JSON
    const std::string jsonFile2 = "jsonToJson2.json";
    {
        EXPECT_TRUE(Save(
        sfmJsonToJson,
        jsonFile2.c_str(),
        ESfM_Data(flags)));
    }
    
    /*********************************************************************/
    /*****************              ABC -> ABC           *****************/
    /*********************************************************************/

    // Export as ABC
    const std::string abcFile = "abcToAbc.abc";
    {
        EXPECT_TRUE(Save(
        sfm_data,
        abcFile.c_str(),              
        ESfM_Data(flags)));
    }

    // Reload
    SfM_Data sfmAbcToAbc;
    {
        EXPECT_TRUE(Load(sfmAbcToAbc, abcFile, ESfM_Data(flags)));
        std::string abcFile2 = "abcToJson.json";
        EXPECT_TRUE(Save(
        sfmAbcToAbc,
        abcFile2.c_str(),
        ESfM_Data(flags)));
        EXPECT_TRUE(sfm_data == sfmAbcToAbc);
    }

    // Export as ABC
    const std::string abcFile2 = "abcToAbc2.abc";
    {
        EXPECT_TRUE(Save(
        sfmAbcToAbc,
        abcFile2.c_str(),
        ESfM_Data(flags)));
    }



    /*********************************************************************/
    /****************      JSON -> ABC -> ABC -> JSON       **************/
    /*********************************************************************/

    // Export as JSON
    const std::string jsonFile3 = "jsonToABC.json";
    {
        EXPECT_TRUE(Save(
        sfm_data,
        jsonFile3.c_str(),
        ESfM_Data(flags)));
    }

    // Reload
    SfM_Data sfmJsonToABC;
    {
        EXPECT_TRUE(Load(sfmJsonToABC, jsonFile3, ESfM_Data(flags)));
        EXPECT_EQ( sfm_data.views.size(), sfmJsonToABC.views.size());
        EXPECT_EQ( sfm_data.GetPoses().size(), sfmJsonToABC.GetPoses().size());
        EXPECT_EQ( sfm_data.intrinsics.size(), sfmJsonToABC.intrinsics.size());
        EXPECT_EQ( sfm_data.structure.size(), sfmJsonToABC.structure.size());
        EXPECT_EQ( sfm_data.control_points.size(), sfmJsonToABC.control_points.size());
    }

    // Export as ABC
    const std::string abcFile3 = "jsonToABC.abc";
    {
        EXPECT_TRUE(Save(
        sfmJsonToABC,
        abcFile3.c_str(),
        ESfM_Data(flags)));
    }

    // Reload
    SfM_Data sfmJsonToABC2;
    {
        EXPECT_TRUE(Load(sfmJsonToABC2, abcFile3, ESfM_Data(flags)));
        EXPECT_EQ( sfm_data.views.size(), sfmJsonToABC2.views.size());
        EXPECT_EQ( sfm_data.GetPoses().size(), sfmJsonToABC2.GetPoses().size());
        EXPECT_EQ( sfm_data.intrinsics.size(), sfmJsonToABC2.intrinsics.size());
        EXPECT_EQ( sfm_data.structure.size(), sfmJsonToABC2.structure.size());
        EXPECT_EQ( sfm_data.control_points.size(), sfmJsonToABC2.control_points.size());
    }

    // Export as ABC
    const std::string abcFile4 = "jsonToABC2.abc";
    {
        EXPECT_TRUE(Save(
        sfmJsonToABC2,
        abcFile4.c_str(),
        ESfM_Data(flags)));
    }

    // Reload
    SfM_Data sfmJsonToABC3;
    {
        EXPECT_TRUE(Load(sfmJsonToABC3, abcFile4, ESfM_Data(flags)));
        EXPECT_EQ( sfm_data.views.size(), sfmJsonToABC3.views.size());
        EXPECT_EQ( sfm_data.GetPoses().size(), sfmJsonToABC3.GetPoses().size());
        EXPECT_EQ( sfm_data.intrinsics.size(), sfmJsonToABC3.intrinsics.size());
        EXPECT_EQ( sfm_data.structure.size(), sfmJsonToABC3.structure.size());
        EXPECT_EQ( sfm_data.control_points.size(), sfmJsonToABC3.control_points.size());
    }

    // Export as JSON
    const std::string jsonFile4 = "jsonToABC2.json";
    {
        EXPECT_TRUE(Save(
        sfmJsonToABC3,
        jsonFile4.c_str(),
        ESfM_Data(flags)));
    }

}

/* ************************************************************************* */
int main() { TestResult tr; return TestRegistry::runAllTests(tr);}
/* ************************************************************************* */

