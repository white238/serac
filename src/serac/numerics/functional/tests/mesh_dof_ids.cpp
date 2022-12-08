#include "mfem.hpp"

#include <iostream>

enum class Family { H1, Hcurl, DG };

bool isH1(const mfem::FiniteElementSpace& fes) {
  return (fes.FEColl()->GetContType() == mfem::FiniteElementCollection::CONTINUOUS);
}

bool isHcurl(const mfem::FiniteElementSpace& fes) {
  return (fes.FEColl()->GetContType() == mfem::FiniteElementCollection::TANGENTIAL);
}

bool isDG(const mfem::FiniteElementSpace& fes) {
  return (fes.FEColl()->GetContType() == mfem::FiniteElementCollection::DISCONTINUOUS);
}

mfem::Mesh patch_test_mesh(mfem::Geometry::Type geom) {
    mfem::Mesh output;
    switch (geom) {
        case mfem::Geometry::Type::TRIANGLE: 
            output = mfem::Mesh(2, 5, 4);
            output.AddVertex(0.0, 0.0);
            output.AddVertex(1.0, 0.0);
            output.AddVertex(1.0, 1.0);
            output.AddVertex(0.0, 1.0);
            output.AddVertex(0.7, 0.4);

            output.AddTriangle(0, 1, 4);
            output.AddTriangle(1, 2, 4);
            output.AddTriangle(2, 3, 4);
            output.AddTriangle(3, 0, 4);
            break;
        case mfem::Geometry::Type::SQUARE:
            output = mfem::Mesh(2, 8, 5);
            output.AddVertex(0.0, 0.0);
            output.AddVertex(1.0, 0.0);
            output.AddVertex(1.0, 1.0);
            output.AddVertex(0.0, 1.0);
            output.AddVertex(0.2, 0.3);
            output.AddVertex(0.6, 0.3);
            output.AddVertex(0.7, 0.8);
            output.AddVertex(0.4, 0.7);

            output.AddQuad(0, 1, 5, 4);
            output.AddQuad(1, 2, 6, 5);
            output.AddQuad(2, 3, 7, 6);
            output.AddQuad(3, 0, 4, 7);
            output.AddQuad(4, 5, 6, 7);
            break;
        case mfem::Geometry::Type::TETRAHEDRON:
            output = mfem::Mesh(3, 9, 12);
            output.AddVertex(0.0, 0.0, 0.0);
            output.AddVertex(1.0, 0.0, 0.0);
            output.AddVertex(1.0, 1.0, 0.0);
            output.AddVertex(0.0, 1.0, 0.0);
            output.AddVertex(0.0, 0.0, 1.0);
            output.AddVertex(1.0, 0.0, 1.0);
            output.AddVertex(1.0, 1.0, 1.0);
            output.AddVertex(0.0, 1.0, 1.0);
            output.AddVertex(0.4, 0.6, 0.7);

            output.AddTet(0, 1, 2, 8); 
            output.AddTet(0, 2, 3, 8); 
            output.AddTet(4, 5, 1, 8); 
            output.AddTet(4, 1, 0, 8); 
            output.AddTet(5, 6, 2, 8); 
            output.AddTet(5, 2, 1, 8); 
            output.AddTet(6, 7, 3, 8); 
            output.AddTet(6, 3, 2, 8); 
            output.AddTet(7, 4, 0, 8); 
            output.AddTet(7, 0, 3, 8); 
            output.AddTet(7, 6, 5, 8); 
            output.AddTet(7, 5, 4, 8);
            break;
        case mfem::Geometry::Type::CUBE:
            output = mfem::Mesh(3, 16, 7);
            output.AddVertex(0.0, 0.0, 0.0); 
            output.AddVertex(1.0, 0.0, 0.0); 
            output.AddVertex(1.0, 1.0, 0.0); 
            output.AddVertex(0.0, 1.0, 0.0); 
            output.AddVertex(0.0, 0.0, 1.0); 
            output.AddVertex(1.0, 0.0, 1.0); 
            output.AddVertex(1.0, 1.0, 1.0); 
            output.AddVertex(0.0, 1.0, 1.0); 
            output.AddVertex(0.2, 0.3, 0.3); 
            output.AddVertex(0.7, 0.5, 0.3); 
            output.AddVertex(0.7, 0.7, 0.3); 
            output.AddVertex(0.3, 0.8, 0.3); 
            output.AddVertex(0.3, 0.4, 0.7); 
            output.AddVertex(0.7, 0.2, 0.6); 
            output.AddVertex(0.7, 0.6, 0.7); 
            output.AddVertex(0.2, 0.7, 0.6);

            output.AddHex( 0,  1,  2,  3,  8,  9, 10, 11);
            output.AddHex( 4,  5,  1,  0, 12, 13,  9,  8);
            output.AddHex( 5,  6,  2,  1, 13, 14, 10,  9);
            output.AddHex( 6,  7,  3,  2, 14, 15, 11, 10);
            output.AddHex( 7,  4,  0,  3, 15, 12,  8, 11);
            output.AddHex(12, 13, 14, 15,  4,  5,  6,  7);
            output.AddHex( 8,  9, 10, 11, 12, 13, 14, 15);
            break;
        
        default:
            std::cout << "patch_test_mesh(): unsupported geometry type" << std::endl;
            exit(1);
            break;
    }
    output.FinalizeMesh();
    return output;
}

std::string to_string(Family f) {
    if (f == Family::H1) return "H1";
    if (f == Family::Hcurl) return "Hcurl";
    if (f == Family::DG) return "DG";
    return "";
}

std::string to_string(mfem::Geometry::Type geom) {
    if (geom == mfem::Geometry::Type::TRIANGLE) return "Triangle";
    if (geom == mfem::Geometry::Type::TETRAHEDRON) return "Tetrahedron";
    if (geom == mfem::Geometry::Type::SQUARE) return "Quadrilateral";
    if (geom == mfem::Geometry::Type::CUBE) return "Hexahedron";
    return "";
}

mfem::Geometry::Type face_type(mfem::Geometry::Type geom) {
    if (geom == mfem::Geometry::Type::TRIANGLE) { return mfem::Geometry::Type::SEGMENT; }
    if (geom == mfem::Geometry::Type::SQUARE) {  return mfem::Geometry::Type::SEGMENT; } 
    if (geom == mfem::Geometry::Type::TETRAHEDRON) {  return mfem::Geometry::Type::TRIANGLE; } 
    if (geom == mfem::Geometry::Type::CUBE) { return mfem::Geometry::Type::SQUARE; }
    return mfem::Geometry::Type::INVALID;
}


struct DoF {
    uint64_t sign : 1;
    uint64_t orientation : 4;
    uint64_t index : 48;
};

template < typename T >
struct Range{
    T * begin() { return ptr[0]; }
    T * end() { return ptr[1]; }
    T * ptr[2];
};

template < typename T >
struct Array2D {
    Array2D() : values{}, dim{} {};
    Array2D(uint64_t m, uint64_t n) : values(m * n, 0), dim{m, n} {};
    Array2D(std::vector<T> && data, uint64_t m, uint64_t n) : values(data), dim{m, n} {};
    Range<T> operator()(int i) { return Range<T>{&values[i * dim[1]], &values[(i + 1) * dim[1]]}; }
    T & operator()(int i, int j) { return values[i * dim[1] + j]; }
    std::vector < T > values;
    uint64_t dim[2];
};


std::vector< std::vector< int > > lexicographic_permutations(int p) {

    std::vector< std::vector< int > > output(mfem::Geometry::Type::NUM_GEOMETRIES);

    {
        auto P = mfem::H1_SegmentElement(p).GetLexicographicOrdering();
        std::vector< int > native_to_lex(P.Size());
        for (int i = 0; i < P.Size(); i++) {
            native_to_lex[i] = P[i];
        }
        output[mfem::Geometry::Type::SEGMENT] = native_to_lex;
    }

    {
        auto P = mfem::H1_TriangleElement(p).GetLexicographicOrdering();
        std::vector< int > native_to_lex(P.Size());
        for (int i = 0; i < P.Size(); i++) {
            native_to_lex[i] = P[i];
        }
        output[mfem::Geometry::Type::TRIANGLE] = native_to_lex;
    }

    {
        auto P = mfem::H1_QuadrilateralElement(p).GetLexicographicOrdering();
        std::vector< int > native_to_lex(P.Size());
        for (int i = 0; i < P.Size(); i++) {
            native_to_lex[i] = P[i];
        }
        output[mfem::Geometry::Type::SQUARE] = native_to_lex;
    }

    {
        auto P = mfem::H1_TetrahedronElement(p).GetLexicographicOrdering();
        std::vector< int > native_to_lex(P.Size());
        for (int i = 0; i < P.Size(); i++) {
            native_to_lex[i] = P[i];
        }
        output[mfem::Geometry::Type::TETRAHEDRON] = native_to_lex;
    }

    {
        auto P = mfem::H1_HexahedronElement(p).GetLexicographicOrdering();
        std::vector< int > native_to_lex(P.Size());
        for (int i = 0; i < P.Size(); i++) {
            native_to_lex[i] = P[i];
        }
        output[mfem::Geometry::Type::CUBE] = native_to_lex;
    }

    // other geometries are not defined, as they are not currently used

    return output;

}

Array2D< int > face_permutations(mfem::Geometry::Type geom, int p) {

    if (geom == mfem::Geometry::Type::SEGMENT) {
        Array2D< int > output(2, p+1);
        for (int i = 0; i <= p; i++) {
            output(0, i) = i;
            output(1, i) = p - i;
        }
        return output;
    }

    if (geom == mfem::Geometry::Type::TRIANGLE) {
        // v = {{0, 0}, {1, 0}, {0, 1}};
        // f = Transpose[{{0, 1, 2}, {1, 0, 2}, {2, 0, 1}, {2, 1, 0}, {1, 2, 0}, {0, 2, 1}} + 1];
        // p v[[f[[1]]]] +  (v[[f[[2]]]] - v[[f[[1]]]]) i + (v[[f[[3]]]] - v[[f[[1]]]]) j
        //
        // {{i, j}, {p-i-j, j}, {j, p-i-j}, {i, p-i-j}, {p-i-j, i}, {j, i}}
        Array2D< int > output(6, (p+1)*(p+2)/2);
        auto tri_id = [p](int x, int y) { return x + ((3 + 2 * p - y) * y) / 2; };
        for (int j = 0; j <= p; j++) {
            for (int i = 0; i <= p - j; i++) {
                int id = tri_id(i,j);
                output(0, tri_id(    i,    j)) = id;
                output(1, tri_id(p-i-j,    j)) = id;
                output(2, tri_id(    j,p-i-j)) = id;
                output(3, tri_id(    i,p-i-j)) = id;
                output(4, tri_id(p-i-j,    i)) = id;
                output(5, tri_id(    j,    i)) = id;
            }
        }
        return output;
    }

    if (geom == mfem::Geometry::Type::SQUARE) {
        // v = {{0, 0}, {1, 0}, {1, 1}, {0, 1}}; 
        // f = Transpose[{{0, 1, 2, 3}, {0, 3, 2, 1}, {1, 2, 3, 0}, {1, 0, 3, 2},
        //                {2, 3, 0, 1}, {2, 1, 0, 3}, {3, 0, 1, 2}, {3, 2, 1, 0}} + 1];
        // p v[[f[[1]]]] +  (v[[f[[2]]]] - v[[f[[1]]]]) i + (v[[f[[4]]]] - v[[f[[1]]]]) j
        //
        // {{i,j}, {j,i}, {p-j,i}, {p-i,j}, {p-i, p-j}, {p-j, p-i}, {j, p-i}, {i, p-j}}
        Array2D< int > output(8, (p+1)*(p+1));
        auto quad_id = [p](int x, int y) { return ((p+1) * y) + x; };
        for (int j = 0; j <= p; j++) {
            for (int i = 0; i <= p; i++) {
                int id = quad_id(i,j);
                output(0, quad_id(  i,   j)) = id;
                output(1, quad_id(  j,   i)) = id;
                output(2, quad_id(p-j,   i)) = id;
                output(3, quad_id(p-i,   j)) = id;
                output(4, quad_id(p-i, p-j)) = id;
                output(5, quad_id(p-j, p-i)) = id;
                output(6, quad_id(  j, p-i)) = id;
                output(7, quad_id(  i, p-j)) = id;
            }
        }
        return output;
    }

    std::cout << "face_permutation(): unsupported geometry type" << std::endl;
    exit(1);

}

std::vector< Array2D<int> > geom_local_face_dofs(int p) {

    // FullSimplify[InterpolatingPolynomial[{
    //   {{0, 2}, (p + 1) + p},
    //   {{0, 1}, p + 1}, {{1, 1}, p + 2},
    //   {{0, 0}, 0}, {{1, 0}, 1}, {{2, 0}, 2}
    // }, {x, y}]]
    // 
    // x + 1/2 (3 + 2 p - y) y
    auto tri_id = [p](int x, int y) { return x + ((3 + 2 * p - y) * y) / 2; };

    // FullSimplify[InterpolatingPolynomial[{
    //  {{0, 3}, ((p - 1) (p) + (p) (p + 1) + (p + 1) (p + 2))/2},
    //  {{0, 2}, ((p) (p + 1) + (p + 1) (p + 2))/2}, {{1, 2},  p - 1 + ((p) (p + 1) + (p + 1) (p + 2))/2},
    //  {{0, 1}, (p + 1) (p + 2)/2}, {{1, 1}, p + (p + 1) (p + 2)/2}, {{2, 1}, 2 p - 1 + (p + 1) (p + 2)/2},
    //  {{0, 0}, 0}, {{1, 0}, p + 1}, {{2, 0}, 2 p + 1}, {{3, 0}, 3 p}
    // }, {y, z}]] + x
    // 
    // x + (z (11 + p (12 + 3 p) - 6 y + z (z - 6 - 3 p)) - 3 y (y - 2 p - 3))/6  
    auto tet_id = [p](int x, int y, int z) { 
        return x + (z*(11+p*(12+3*p) - 6*y + z*(z-6-3*p)) - 3*y*(y-2*p-3)) / 6;
    };

    auto quad_id = [p](int x, int y) { return ((p+1) * y) + x; };

    auto hex_id = [p](int x, int y, int z) { return (p+1) * ((p+1) * z + y) + x; };

    std::vector<Array2D< int > > output(mfem::Geometry::Type::NUM_GEOMETRIES);

    Array2D<int> tris(3, p+1);
    for (int k = 0; k <= p; k++) {
        tris(0, k) = tri_id(k, 0);
        tris(1, k) = tri_id(p-k, k);
        tris(2, k) = tri_id(0, p-k);
    }
    output[mfem::Geometry::Type::TRIANGLE] = tris;

    Array2D<int> quads(4, p+1);
    for (int k = 0; k <= p; k++) {
        quads(0, k) = quad_id(k, 0);
        quads(1, k) = quad_id(p, k);
        quads(2, k) = quad_id(p - k, k);
        quads(3, k) = quad_id(0, p - k);
    }
    output[mfem::Geometry::Type::SQUARE] = quads;

    // v = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    // f = Transpose[{{1, 2, 3}, {0, 3, 2}, {0, 1, 3}, {0, 2, 1}} + 1];
    // p v[[f[[1]]]] +  (v[[f[[2]]]] - v[[f[[1]]]]) j + (v[[f[[3]]]] - v[[f[[1]]]]) k
    //
    // {{p-j-k, j, k}, {0, k, j}, {j, 0, k}, {k, j, 0}}
    Array2D<int> tets(4, (p+1) * (p+2) / 2);
    for (int k = 0; k <= p; k++) {
        for (int j = 0; j <= p - k; j++) {
            int id = tri_id(j,k);
            tets(0, id) = tet_id(p - j - k, j, k);
            tets(1, id) = tet_id(        0, k, j);
            tets(2, id) = tet_id(        j, 0, k);
            tets(3, id) = tet_id(        k, j, 0);
        }
    }
    output[mfem::Geometry::Type::TETRAHEDRON] = tets;

    // v = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, 
    //      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    // f = Transpose[{{3, 2, 1, 0}, {0, 1, 5, 4}, {1, 2, 6, 5}, 
    //               {2, 3, 7, 6}, {3, 0, 4, 7}, {4, 5, 6, 7}} + 1]; 
    // p v[[f[[1]]]] +  (v[[f[[2]]]] - v[[f[[1]]]]) j + (v[[f[[4]]]] - v[[f[[1]]]]) k
    //
    // {{j, p-k, 0}, {j, 0, k}, {p, j, k}, {p-j, p, k}, {0, p-j, k}, {j, k, p}}
    Array2D<int> hexes(6, (p+1) * (p+1));
    for (int k = 0; k <= p; k++) {
        for (int j = 0; j <= p; j++) {
            int id = quad_id(j,k);
            hexes(0, id) = hex_id(    j, p - k, 0);
            hexes(1, id) = hex_id(    j,     0, k);
            hexes(2, id) = hex_id(    p,     j, k);
            hexes(3, id) = hex_id(p - j,     p, k);
            hexes(4, id) = hex_id(    0, p - j, k);
            hexes(5, id) = hex_id(    j,     k, p);
        }
    }
    output[mfem::Geometry::Type::CUBE] = hexes;

    return output;

}

Array2D< int > GetBoundaryFaceDofs(mfem::FiniteElementSpace * fes, mfem::Geometry::Type face_geom) {

    std::vector < int > face_dofs;
    mfem::Mesh * mesh = fes->GetMesh();
    mfem::Table * face_to_elem = mesh->GetFaceToElementTable(); 

    // note: this assumes that all the elements are the same polynomial order
    int p = fes->GetElementOrder(0);
    Array2D<int> face_perm = face_permutations(face_geom, p);
    std::vector< Array2D<int> > local_face_dofs = geom_local_face_dofs(p);
    std::vector< std::vector<int> > elem_perm = lexicographic_permutations(p);

    uint64_t n = 0;

    for (int f = 0; f < fes->GetNF(); f++) {

        // don't bother with interior faces, or faces with the wrong geometry
        if (mesh->FaceIsInterior(f) || mesh->GetFaceGeometryType(f) != face_geom) {
          continue;
        }

        // mfem doesn't provide this connectivity info for DG spaces directly,
        // so we have to get at it indirectly in several steps:
        if (isDG(*fes)) {
        
            // 1. find the element that this face belongs to
            mfem::Array<int> elem_ids;
            face_to_elem->GetRow(f, elem_ids);

            // 2. find `i` such that `elem_side_ids[i] == f`
            mfem::Array<int> elem_side_ids, orientations;
            if (mesh->Dimension() == 2) {

                mesh->GetElementEdges(elem_ids[0], elem_side_ids, orientations);

                // mfem returns {-1, 1} for edge orientations,
                // but {0, 1, ... , n} for face orientations.
                // Here, we renumber the edge orientations to 
                // {0 (no permutation), 1 (reversed)} so the values can be
                // consistently used as indices into a permutation table
                for (auto & o : orientations) { o = (o == -1) ? 1 : 0; }

            } else {

                mesh->GetElementFaces(elem_ids[0], elem_side_ids, orientations);

            }

            int i;
            for (i = 0; i < elem_side_ids.Size(); i++) {
                if (elem_side_ids[i] == f) break;
            }

            // 3. get the dofs for the entire element
            mfem::Array<int> elem_dof_ids;
            fes->GetElementDofs(elem_ids[0], elem_dof_ids);
            
            mfem::Geometry::Type elem_geom = mesh->GetElementGeometry(elem_ids[0]);
            std::cout << "face " << f << " belongs to element " << elem_ids[0];
            std::cout << " with local face id " << i << " and orientation " << orientations[i] << std::endl;

            for (auto dof : elem_dof_ids) {
                std::cout << dof << " ";
            }
            std::cout << std::endl;

            // 4. extract only the dofs that correspond to side `i`
            for (auto k : local_face_dofs[elem_geom](i)) {
                face_dofs.push_back(elem_dof_ids[k]);
                std::cout << elem_dof_ids[k] << " ";
            }
            std::cout << std::endl;

        // H1 and Hcurl spaces are more straight-forward, since
        // we can use FiniteElementSpace::GetFaceDofs() directly
        } else {

            mfem::Array<int> dofs;
            fes->GetFaceDofs(f, dofs);

            for (int k = 0; k < dofs.Size(); k++) {
                face_dofs.push_back(dofs[elem_perm[face_geom][k]]);
            }

        }

        n++;

    }

    delete face_to_elem;

    uint64_t dofs_per_face = face_dofs.size() / n;

    return Array2D<int>(std::move(face_dofs), n, dofs_per_face);

}

//#define ENABLE_GLVIS

mfem::FiniteElementCollection * makeFEC(Family family, int order, int dim) {
    switch (family) {
      case Family::H1:
        return new mfem::H1_FECollection(order, dim);
        break;
      case Family::Hcurl:
        return new mfem::ND_FECollection(order, dim);
        break;
      case Family::DG:
        return new mfem::L2_FECollection(order, dim, mfem::BasisType::GaussLobatto);
        break;
    }
    return nullptr;
}

int main() {

    int order = 3;

    mfem::Geometry::Type geometries[] = {
        mfem::Geometry::Type::TRIANGLE, 
        mfem::Geometry::Type::SQUARE, 
        mfem::Geometry::Type::TETRAHEDRON, 
        mfem::Geometry::Type::CUBE};

    //auto identity_func = [](const mfem::Vector & in, double, mfem::Vector & out) {
    //    out = in;
    //};

    auto x_func = [](const mfem::Vector & in, double) { return in[0]; };
    auto y_func = [](const mfem::Vector & in, double) { return in[1]; };
    auto z_func = [](const mfem::Vector & in, double) { return in[2]; };

    #if defined ENABLE_GLVIS
    char vishost[] = "localhost";
    int  visport   = 19916;
    #endif

    for (auto geom : geometries) {

        std::cout << to_string(geom) << std::endl;

        mfem::Mesh mesh = patch_test_mesh(geom);
        const int  dim = mesh.Dimension();

        #if defined ENABLE_GLVIS
        mfem::socketstream sol_sock(vishost, visport);
        sol_sock.precision(8);
        sol_sock << "mesh\n" << mesh << std::flush;
        #endif

        auto * H1fec = makeFEC(Family::H1, order, dim);
        auto * L2fec = makeFEC(Family::DG, order, dim);
        //auto * Hcurlfec = makeFEC(Family::Hcurl, order, dim);

        mfem::FiniteElementSpace H1fes(&mesh, H1fec, 1, mfem::Ordering::byVDIM);
        mfem::FiniteElementSpace L2fes(&mesh, L2fec, 1, mfem::Ordering::byVDIM);

        //std::cout << "volume elements: " << std::endl;
        //for (int i = 0; i < fes.GetNE(); i++) {
        //    mfem::Array<int> vdofs;
        //    fes.GetElementVDofs(i, vdofs);
        //}

        mfem::FunctionCoefficient x(x_func);
        mfem::FunctionCoefficient y(y_func);
        mfem::FunctionCoefficient z(z_func);
        mfem::GridFunction H1_x(&H1fes);
        mfem::GridFunction L2_x(&L2fes);
        H1_x.ProjectCoefficient(x);
        L2_x.ProjectCoefficient(x);

        auto H1_face_dof_ids = GetBoundaryFaceDofs(&H1fes, face_type(geom));
        auto L2_face_dof_ids = GetBoundaryFaceDofs(&L2fes, face_type(geom));

        H1_x.Print(std::cout, 64);
        L2_x.Print(std::cout, 64);

        //std::cout << H1_face_dof_ids.dim[0] << " " << H1_face_dof_ids.dim[1] << std::endl;
        //std::cout << L2_face_dof_ids.dim[0] << " " << L2_face_dof_ids.dim[1] << std::endl;
        //std::cout << std::endl;

        uint64_t n0 = H1_face_dof_ids.dim[0];
        uint64_t n1 = H1_face_dof_ids.dim[1];
        for (uint64_t i = 0; i < n0; i++) {
            std::cout << i << ": " << std::endl;
            for (uint64_t j = 0; j < n1; j++) {
                std::cout << H1_x(H1_face_dof_ids(int(i),int(j))) << " ";
            }
            std::cout << std::endl;

            for (uint64_t j = 0; j < n1; j++) {
                std::cout << L2_x(L2_face_dof_ids(int(i),int(j))) << " ";
            }
            std::cout << std::endl;
        }

    }

}
