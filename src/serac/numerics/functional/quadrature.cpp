#include "serac/numerics/functional/quadrature.hpp"

#include <map>

namespace serac {

  QuadratureRule GaussLegendreRule(mfem::Geometry::Type geom, PointsPerDimension points) {

    auto n = points.q;

    QuadratureRule rule;

    if (geom == mfem::Geometry::SEGMENT) {
      switch (n) {
        case 1: 
          rule.points = {0.50000000000000000};
          rule.weights = {1.000000000000000};
          break;
        case 2: 
          rule.points = {0.2113248654051871, 0.7886751345948129};
          rule.weights = {0.500000000000000, 0.500000000000000};
          break;
        case 3: 
          rule.points = {0.1127016653792583, 0.500000000000000, 0.887298334620742};
          rule.weights = {0.277777777777778, 0.444444444444444, 0.277777777777778};
          break;
        case 4: 
          rule.points = {0.0694318442029737, 0.330009478207572, 0.669990521792428, 0.930568155797026};
          rule.weights = {0.173927422568727, 0.326072577431273, 0.326072577431273, 0.173927422568727};
          break;
        case 5: 
          rule.points = {0.0469100770306680, 0.230765344947158, 0.500000000000000, 0.769234655052842, 0.953089922969332};
          rule.weights = {0.118463442528095, 0.239314335249683, 0.284444444444444, 0.239314335249683, 0.118463442528095};
          break;
        case 6: 
          rule.points = {0.0337652428984240, 0.169395306766868, 0.380690406958402, 0.619309593041598, 0.830604693233132, 0.966234757101576};
          rule.weights = {0.085662246189585, 0.180380786524069, 0.233956967286346, 0.233956967286346, 0.180380786524069, 0.085662246189585};
          break;
        case 7: 
          rule.points = {0.0254460438286207, 0.129234407200303, 0.297077424311301, 0.500000000000000, 0.702922575688699, 0.87076559279970, 0.97455395617138};
          rule.weights = {0.0647424830844348, 0.139852695744638, 0.190915025252559, 0.208979591836735, 0.190915025252559,  0.139852695744638, 0.0647424830844348};
          break;
        case 8: 
          rule.points = {0.0198550717512319, 0.101666761293187, 0.237233795041836, 0.408282678752175, 0.591717321247825, 0.76276620495816, 0.89833323870681, 0.98014492824877};
          rule.weights = {0.0506142681451881, 0.111190517226687, 0.156853322938944, 0.181341891689181, 0.181341891689181,  0.156853322938944, 0.111190517226687, 0.0506142681451881};
          break;
      }
    }
  
    if (geom == mfem::Geometry::TRIANGLE) {
      switch (n) {
        case 1: 
          rule.points = {0.333333333333333, 0.333333333333333};
          rule.weights = {};
          break;
        case 2: 
          rule.points = {0.166666666666667, 0.166666666666667,
                         0.166666666666667, 0.666666666666667,
                         0.666666666666667, 0.166666666666667};
          rule.weights = {};
          break;
        case 3: 
          rule.points = {0.09157621350978, 0.09157621350978,
                         0.09157621350978, 0.81684757298044,
                         0.81684757298044, 0.09157621350978,
                         0.108103018168071, 0.445948490915964,
                         0.445948490915964, 0.108103018168071,
                         0.445948490915964, 0.445948490915964};
          rule.weights = {};
          break;
        case 4: 
          rule.points = {0.055564052669793, 0.055564052669793,
                         0.055564052669793, 0.888871894660413,
                         0.888871894660413, 0.055564052669793,
                         0.070255540518384, 0.634210747745723,
                         0.634210747745723, 0.070255540518384,
                         0.634210747745723, 0.295533711735893,
                         0.070255540518384, 0.295533711735893,
                         0.295533711735893, 0.070255540518384,
                         0.295533711735893, 0.634210747745723,
                         0.333333333333333, 0.333333333333333};
          rule.weights = {};
          break;
        case 5: 
          rule.points = {0.035870877695734000590, 0.035870877695734000590,
                         0.035870877695734000590, 0.92825824460853301190,
                         0.92825824460853301190, 0.035870877695734000590,
                         0.24172939576796700911, 0.24172939576796700911,
                         0.24172939576796700911, 0.51654120846406603729,
                         0.51654120846406603729, 0.24172939576796700911,
                         0.051382424445842997396, 0.47430878777707902172,
                         0.47430878777707902172, 0.051382424445842997396,
                         0.47430878777707902172, 0.47430878777707902172,
                         0.047312487011716003460, 0.75118363110648400660,
                         0.75118363110648400660, 0.047312487011716003460,
                         0.75118363110648400660, 0.20150388188179998994,
                         0.047312487011716003460, 0.20150388188179998994,
                         0.20150388188179998994, 0.047312487011716003460,
                         0.20150388188179998994, 0.75118363110648400660};
          rule.weights = {};
          break;
      }
    }
  
    if (geom == mfem::Geometry::TETRAHEDRON) {
      switch (n) {
        case 1: 
          rule.points = {0.25, 0.25, 0.25};
          rule.weights = {};
          break;
        case 2: 
          rule.points = {0.585410196624967936, 0.138196601125011008, 0.138196601125011008,
                         0.138196601125011008, 0.585410196624967936, 0.138196601125011008,
                         0.138196601125011008, 0.138196601125011008, 0.585410196624967936,
                         0.138196601125011008, 0.138196601125011008, 0.138196601125011008};
          rule.weights = {};
          break;
        case 3: 
          rule.points = {0.77849529482132992, 0.0738349017262233984, 0.0738349017262233984,
                         0.0738349017262233984, 0.77849529482132992, 0.0738349017262233984,
                         0.0738349017262233984, 0.0738349017262233984, 0.77849529482132992,
                         0.0738349017262233984, 0.0738349017262233984, 0.0738349017262233984,
                         0.406244343884051008, 0.406244343884051008, 0.0937556561159491072,
                         0.406244343884051008, 0.0937556561159491072, 0.406244343884051008,
                         0.406244343884051008, 0.0937556561159491072, 0.0937556561159491072,
                         0.0937556561159491072, 0.406244343884051008, 0.406244343884051008,
                         0.0937556561159491072, 0.406244343884051008, 0.0937556561159491072,
                         0.0937556561159491072, 0.0937556561159491072, 0.406244343884051008};
          rule.weights = {};
          break;
        case 4: 
          rule.points = {0.902942215818267904, 0.0323525947272438976, 0.0323525947272438976,
                         0.0323525947272438976, 0.902942215818267904, 0.0323525947272438976,
                         0.0323525947272438976, 0.0323525947272438976, 0.902942215818267904,
                         0.0323525947272438976, 0.0323525947272438976, 0.0323525947272438976,
                         0.262682583887778976, 0.616596533061936896, 0.0603604415251420928,
                         0.616596533061936896, 0.262682583887778976, 0.0603604415251420928,
                         0.262682583887778976, 0.0603604415251420928, 0.616596533061936896,
                         0.616596533061936896, 0.0603604415251420928, 0.262682583887778976,
                         0.262682583887778976, 0.0603604415251420928, 0.0603604415251420928,
                         0.616596533061936896, 0.0603604415251420928, 0.0603604415251420928,
                         0.0603604415251420928, 0.262682583887778976, 0.616596533061936896,
                         0.0603604415251420928, 0.616596533061936896, 0.262682583887778976,
                         0.0603604415251420928, 0.262682583887778976, 0.0603604415251420928,
                         0.0603604415251420928, 0.616596533061936896, 0.0603604415251420928,
                         0.0603604415251420928, 0.0603604415251420928, 0.262682583887778976,
                         0.0603604415251420928, 0.0603604415251420928, 0.616596533061936896,
                         0.309769304272861952, 0.309769304272861952, 0.309769304272861952,
                         0.309769304272861952, 0.309769304272861952, 0.0706920871814129024,
                         0.309769304272861952, 0.0706920871814129024, 0.309769304272861952,
                         0.0706920871814129024, 0.309769304272861952, 0.309769304272861952};
          rule.weights = {};
          break;
        case 5: 
          rule.points = {0.91978967333688, 0.0267367755543734976, 0.0267367755543734976,
                         0.0267367755543734976, 0.91978967333688, 0.0267367755543734976,
                         0.0267367755543734976, 0.0267367755543734976, 0.91978967333688,
                         0.0267367755543734976, 0.0267367755543734976, 0.0267367755543734976,
                         0.174035630246894016, 0.747759888481809024, 0.0391022406356488,
                         0.747759888481809024, 0.174035630246894016, 0.0391022406356488,
                         0.174035630246894016, 0.0391022406356488, 0.747759888481809024,
                         0.747759888481809024, 0.0391022406356488, 0.174035630246894016,
                         0.174035630246894016, 0.0391022406356488, 0.0391022406356488,
                         0.747759888481809024, 0.0391022406356488, 0.0391022406356488,
                         0.0391022406356488, 0.174035630246894016, 0.747759888481809024,
                         0.0391022406356488, 0.747759888481809024, 0.174035630246894016,
                         0.0391022406356488, 0.174035630246894016, 0.0391022406356488,
                         0.0391022406356488, 0.747759888481809024, 0.0391022406356488,
                         0.0391022406356488, 0.0391022406356488, 0.174035630246894016,
                         0.0391022406356488, 0.0391022406356488, 0.747759888481809024,
                         0.454754599984483008, 0.454754599984483008, 0.0452454000155171968,
                         0.454754599984483008, 0.0452454000155171968, 0.454754599984483008,
                         0.454754599984483008, 0.0452454000155171968, 0.0452454000155171968,
                         0.0452454000155171968, 0.454754599984483008, 0.454754599984483008,
                         0.0452454000155171968, 0.454754599984483008, 0.0452454000155171968,
                         0.0452454000155171968, 0.0452454000155171968, 0.454754599984483008,
                         0.503118645014598016, 0.223201037962315008, 0.223201037962315008,
                         0.223201037962315008, 0.503118645014598016, 0.223201037962315008,
                         0.223201037962315008, 0.223201037962315008, 0.503118645014598016,
                         0.503118645014598016, 0.223201037962315008, 0.050479279060772,
                         0.223201037962315008, 0.503118645014598016, 0.050479279060772,
                         0.223201037962315008, 0.223201037962315008, 0.050479279060772,
                         0.503118645014598016, 0.050479279060772, 0.223201037962315008,
                         0.223201037962315008, 0.050479279060772, 0.503118645014598016,
                         0.223201037962315008, 0.050479279060772, 0.223201037962315008,
                         0.050479279060772, 0.503118645014598016, 0.223201037962315008,
                         0.050479279060772, 0.223201037962315008, 0.503118645014598016,
                         0.050479279060772, 0.223201037962315008, 0.223201037962315008,
                         0.25, 0.25, 0.25};
          rule.weights = {};
          break;
        case 6: 
          rule.points = {0.955143804540822016, 0.0149520651530592, 0.0149520651530592,
                         0.0149520651530592, 0.955143804540822016, 0.0149520651530592,
                         0.0149520651530592, 0.0149520651530592, 0.955143804540822016,
                         0.0149520651530592, 0.0149520651530592, 0.0149520651530592,
                         0.779976008441539968, 0.151831949165936992, 0.0340960211962614976,
                         0.151831949165936992, 0.779976008441539968, 0.0340960211962614976,
                         0.779976008441539968, 0.0340960211962614976, 0.151831949165936992,
                         0.151831949165936992, 0.0340960211962614976, 0.779976008441539968,
                         0.779976008441539968, 0.0340960211962614976, 0.0340960211962614976,
                         0.151831949165936992, 0.0340960211962614976, 0.0340960211962614976,
                         0.0340960211962614976, 0.779976008441539968, 0.151831949165936992,
                         0.0340960211962614976, 0.151831949165936992, 0.779976008441539968,
                         0.0340960211962614976, 0.779976008441539968, 0.0340960211962614976,
                         0.0340960211962614976, 0.151831949165936992, 0.0340960211962614976,
                         0.0340960211962614976, 0.0340960211962614976, 0.779976008441539968,
                         0.0340960211962614976, 0.0340960211962614976, 0.151831949165936992,
                         0.354934056063979008, 0.552655643106017024, 0.0462051504150017024,
                         0.552655643106017024, 0.354934056063979008, 0.0462051504150017024,
                         0.354934056063979008, 0.0462051504150017024, 0.552655643106017024,
                         0.552655643106017024, 0.0462051504150017024, 0.354934056063979008,
                         0.354934056063979008, 0.0462051504150017024, 0.0462051504150017024,
                         0.552655643106017024, 0.0462051504150017024, 0.0462051504150017024,
                         0.0462051504150017024, 0.354934056063979008, 0.552655643106017024,
                         0.0462051504150017024, 0.552655643106017024, 0.354934056063979008,
                         0.0462051504150017024, 0.354934056063979008, 0.0462051504150017024,
                         0.0462051504150017024, 0.552655643106017024, 0.0462051504150017024,
                         0.0462051504150017024, 0.0462051504150017024, 0.354934056063979008,
                         0.0462051504150017024, 0.0462051504150017024, 0.552655643106017024,
                         0.538104322888002048, 0.228190461068760992, 0.228190461068760992,
                         0.228190461068760992, 0.538104322888002048, 0.228190461068760992,
                         0.228190461068760992, 0.228190461068760992, 0.538104322888002048,
                         0.538104322888002048, 0.228190461068760992, 0.00551475497447749952,
                         0.228190461068760992, 0.538104322888002048, 0.00551475497447749952,
                         0.228190461068760992, 0.228190461068760992, 0.00551475497447749952,
                         0.538104322888002048, 0.00551475497447749952, 0.228190461068760992,
                         0.228190461068760992, 0.00551475497447749952, 0.538104322888002048,
                         0.228190461068760992, 0.00551475497447749952, 0.228190461068760992,
                         0.00551475497447749952, 0.538104322888002048, 0.228190461068760992,
                         0.00551475497447749952, 0.228190461068760992, 0.538104322888002048,
                         0.00551475497447749952, 0.228190461068760992, 0.228190461068760992,
                         0.19618375957456, 0.352305260087993984, 0.352305260087993984,
                         0.352305260087993984, 0.19618375957456, 0.352305260087993984,
                         0.352305260087993984, 0.352305260087993984, 0.19618375957456,
                         0.19618375957456, 0.352305260087993984, 0.0992057202494530048,
                         0.352305260087993984, 0.19618375957456, 0.0992057202494530048,
                         0.352305260087993984, 0.352305260087993984, 0.0992057202494530048,
                         0.19618375957456, 0.0992057202494530048, 0.352305260087993984,
                         0.352305260087993984, 0.0992057202494530048, 0.19618375957456,
                         0.352305260087993984, 0.0992057202494530048, 0.352305260087993984,
                         0.0992057202494530048, 0.19618375957456, 0.352305260087993984,
                         0.0992057202494530048, 0.352305260087993984, 0.19618375957456,
                         0.0992057202494530048, 0.352305260087993984, 0.352305260087993984,
                         0.59656499562101696, 0.134478334792994016, 0.134478334792994016,
                         0.134478334792994016, 0.59656499562101696, 0.134478334792994016,
                         0.134478334792994016, 0.134478334792994016, 0.59656499562101696,
                         0.134478334792994016, 0.134478334792994016, 0.134478334792994016};
          rule.weights = {};
          break;

      }
    } // if TETRAHEDRON

    return rule;

  };

  // TODO
  // TODO
  // TODO
  static std::map< mfem::Geometry::Type, std::vector< uint32_t > > polynomial_order_LUT = {
    {mfem::Geometry::SEGMENT, {1, 2, 3, 4, 5}},
    {mfem::Geometry::TRIANGLE, {1, 2, 3, 4, 5}},
    {mfem::Geometry::TETRAHEDRON, {1, 2, 3, 4, 5}}
  };

  QuadratureRule GaussLegendreRule(mfem::Geometry::Type geom, PolynomialOrder order) {
    return GaussLegendreRule(geom, PointsPerDimension{polynomial_order_LUT[geom][order.p]});
  };

}  // namespace serac
