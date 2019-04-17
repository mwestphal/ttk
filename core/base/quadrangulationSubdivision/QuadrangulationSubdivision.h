/// \ingroup base
/// \class ttk::QuadrangulationSubdivision
/// \author Pierre Guillou <pierre.guillou@lip6.fr>
/// \author Julien Tierny <julien.tierny@lip6.fr>
/// \date March 2019
///
/// \brief TTK processing package for the topological simplification of scalar
/// data.
///
///
///
/// \sa ttkQuadrangulationSubdivision.cpp % for a usage example.

#pragma once

// base code includes
#include <Geometry.h>
#include <MorseSmaleComplex.h>
#include <Triangulation.h>
#include <Wrapper.h>
#include <cmath>
#include <set>
#include <tuple>
#include <type_traits>

namespace ttk {

  class QuadrangulationSubdivision : public Debug {

  public:
    // default constructor
    QuadrangulationSubdivision() = default;
    // default destructor
    ~QuadrangulationSubdivision() override = default;
    // default copy constructor
    QuadrangulationSubdivision(const QuadrangulationSubdivision &) = default;
    // default move constructor
    QuadrangulationSubdivision(QuadrangulationSubdivision &&) = default;
    // default copy assignment operator
    QuadrangulationSubdivision &operator=(const QuadrangulationSubdivision &)
      = default;
    // default move assignment operator
    QuadrangulationSubdivision &operator=(QuadrangulationSubdivision &&)
      = default;

    inline void setSubdivisionLevel(const unsigned int value) {
      subdivisionLevel_ = value;
    }
    inline void setRelaxationIterations(const unsigned int value) {
      relaxationIterations_ = value;
    }
    inline void setLockInputExtrema(const bool value) {
      lockInputExtrema = value;
    }
    inline void setLockAllInputVertices(const bool value) {
      lockAllInputVertices = value;
    }
    inline void setInputQuads(void *const address, unsigned int size) {
      inputQuads_ = static_cast<Quad *>(address);
      inputQuadNumber_ = size;
    }
    inline void setInputVertices(void *const address, unsigned int size) {
      inputVertices_ = static_cast<Point *>(address);
      inputVertexNumber_ = size;
    }
    inline void setInputVertexIdentifiers(void *const address,
                                          unsigned int size) {
      nearestVertexIdentifier_.resize(size);
      auto inputVertexIdentifiers = static_cast<SimplexId *>(address);
      for(size_t i = 0; i < size; i++) {
        nearestVertexIdentifier_[i] = inputVertexIdentifiers[i];
      }
    }
    inline void setupTriangulation(Triangulation *const triangl) {
      triangulation_ = triangl;
      if(triangulation_ != nullptr) {
        vertexNumber_ = triangulation_->getNumberOfVertices();
        triangulation_->preprocessVertexNeighbors();
        triangulation_->preprocessVertexTriangles();
      }
    }
    inline void setOutputQuads(std::vector<long long> *const quads) {
      outputQuads_ = reinterpret_cast<std::vector<Quad> *>(quads);
    }
    inline void setOutputPoints(std::vector<float> *const address) {
      outputPoints_ = reinterpret_cast<std::vector<Point> *>(address);
    }
    inline void setOutputValences(std::vector<SimplexId> *const address) {
      outputValences_ = address;
    }
    inline void setOutputInfos(std::vector<SimplexId> *const address) {
      outputVertType_ = address;
    }

    int execute();

  private:
    // vtkPoint instance with interleaved coordinates (AoS)
    struct Point {
      float x;
      float y;
      float z;
      Point operator+(const Point other) const {
        Point res{};
        res.x = x + other.x;
        res.y = y + other.y;
        res.z = z + other.z;
        return res;
      }
      Point operator*(const float scalar) const {
        Point res{};
        res.x = x * scalar;
        res.y = y * scalar;
        res.z = z * scalar;
        return res;
      }
      Point operator-(Point other) const {
        return *this + other * (-1);
      }
      Point operator/(const float scalar) const {
        return (*this * (1.0f / scalar));
      }
    };
    // VTK_QUAD representation with vtkIdType
    struct Quad {
      long long n; // number of vertices, 4
      long long i; // index of first vertex
      long long j; // second vertex
      long long k; // third vertex
      long long l; // fourth vertex
    };

    /**
     * @brief Subdivise a quadrangular mesh
     *
     * Add five new points per existing quadrangle:
     * - four at the middle of the four quadrangle edges
     * - one at the quadrangle barycenter
     *
     * The points are generated once, no duplicate is generated when
     * considering two quadrangles sharing an edge.
     *
     * New quadrangles using these new points are then generated
     * out-of-place in an output vector of quadrangles.
     *
     * @return 0 in case of success
     */
    int subdivise();

    /**
     * @brief Project a generated quadrangle vertex into the
     * triangular input mesh
     *
     * Project a subset of the current quadrangular mesh onto the
     * triangular input mesh.
     *
     * @param[in] filtered Set of indices that should not be projected
     * @return 0 in case of success
     */
    int project(const std::set<size_t> &filtered);

    /**
     * @brief Relax every generated point of a quadrangular mesh
     *
     * Take every generated point of the current quadrangular mesh,
     * and move its position to the barycenter of its neighbors.
     *
     * @param[in] filtered Set of indices that should not be projected
     * @return 0 in case of success
     */
    int relax(const std::set<size_t> &filtered);

    /**
     * @brief Store for every quad vertex its neighbors
     *
     * Each quad vertex should be linked to four other vertices. This
     * functions stores into the quadNeighbors_ member this relation.
     *
     * @param[in] quads Quadrangular mesh to find neighbors in
     * @param[in] secondNeighbors Also store secondary neighbors (quad third
     * vertex)
     *
     * @return 0 in case of success
     */
    int getQuadNeighbors(const std::vector<Quad> &quads,
                         std::vector<std::set<size_t>> &neighbors,
                         const bool secondNeighbors = false);

    /**
     * @brief Compute the projection in the nearest triangle
     *
     * @param[in] i input index of quadrangle vertex
     *
     * @return coordinates of projection
     */
    Point findProjectionInTriangle(SimplexId i);

    /**
     * @brief Find the middle of a quad edge using Dijkstra
     *
     * Minimize the sum of the distance to the two edge vertices, and the
     * distance absolute difference
     *
     * @param[in] a First edge vertex index
     * @param[in] b Second edge vertex index
     *
     * @return TTK identifier of potential edge middle
     */
    SimplexId findEdgeMiddle(const size_t a, const size_t b) const;

    /**
     * @brief Find a quad barycenter using Dijkstra
     *
     * Minimize the sum of the distance to every vertex of the current quad.
     *
     * @param[in] quadVertices Set of quad vertices point ids in which to
     * find a barycenter
     * @param[in] edgeMiddles Vector of parent quad edges middle identifiers
     *
     * @return TTK identifier of potential barycenter
     */
    SimplexId findQuadBary(const std::set<size_t> &quadVertices,
                           const std::vector<SimplexId> &edgeMiddles) const;

    /**
     * @brief Find input vertices with more than 4 neighbors
     *
     * @param[out] output Output set of input extraordinary point indices
     *
     * @return 0 in case of success
     */
    int findExtraordinaryVertices(std::set<size_t> &output);

  protected:
    // number of vertices in the mesh
    SimplexId vertexNumber_{};

    // wanted number of subdivisions of the input quadrangles
    unsigned int subdivisionLevel_{3};
    // number of relaxation iterations
    unsigned int relaxationIterations_{100};
    // lock input extrema
    bool lockInputExtrema{true};
    // lock all input vertices
    bool lockAllInputVertices{true};

    // number of input quadrangles
    unsigned int inputQuadNumber_{};
    // input quadrangles
    Quad *inputQuads_{};

    // number of input points (quad vertices)
    unsigned int inputVertexNumber_{};
    // input quadrangle vertices (3D coordinates)
    Point *inputVertices_{};

    // input triangulation
    Triangulation *triangulation_{};

    // array of output quadrangles
    std::vector<Quad> *outputQuads_{};
    // array of output quadrangle vertices
    std::vector<Point> *outputPoints_{};
    // array of output quadrangle vertex valences
    std::vector<SimplexId> *outputValences_{};
    // array of output quadrangle vertex type
    std::vector<SimplexId> *outputVertType_{};
    // array mapping quadrangle neighbors
    std::vector<std::set<size_t>> quadNeighbors_{};
    // array of nearest input vertex TTK identifier
    std::vector<SimplexId> nearestVertexIdentifier_{};
    // holds geodesic distance to every other quad vertex sharing a quad
    std::vector<std::vector<float>> vertexDistance_{};
  };
} // namespace ttk

// if the package is a pure template typename, uncomment the following line
// #include                  <QuadrangulationSubdivision.cpp>
