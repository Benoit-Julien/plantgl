/* -*-c++-*-
 *  ----------------------------------------------------------------------------
 *
 *       PlantGL: Plant Graphic Library
 *
 *       Copyright 1995-2003 UMR Cirad/Inria/Inra Dap - Virtual Plant Team
 *
 *       File author(s): F. Boudon
 *
 *  ----------------------------------------------------------------------------
 *
 *                      GNU General Public Licence
 *
 *       This program is free software; you can redistribute it and/or
 *       modify it under the terms of the GNU General Public License as
 *       published by the Free Software Foundation; either version 2 of
 *       the License, or (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS For A PARTICULAR PURPOSE. See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public
 *       License along with this program; see the file COPYING. If not,
 *       write to the Free Software Foundation, Inc., 59
 *       Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  ----------------------------------------------------------------------------
 */


#include "pointmanipulation.h"

PGL_USING_NAMESPACE
TOOLS_USING_NAMESPACE

#ifdef WITH_CGAL
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Triangulation_vertex_base_with_info_3.h>
#include <CGAL/Delaunay_triangulation_3.h>
// #include <CGAL/Triangulation_3.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel         TK;
typedef CGAL::Triangulation_vertex_base_with_info_3<uint32_t, TK>   TVb;
typedef CGAL::Triangulation_data_structure_3<TVb>                    Tds;

typedef CGAL::Delaunay_triangulation_3<TK, Tds>                      Triangulation;
// typedef CGAL::Triangulation_3<K,Tds>      Triangulation;


typedef Triangulation::Cell_handle    TCell_handle;
typedef Triangulation::Vertex_handle  TVertex_handle;
typedef Triangulation::Locate_type    TLocate_type;
typedef Triangulation::Point          TPoint;
typedef Triangulation::Segment        TSegment;


template<class CGALPoint>
inline CGALPoint toPoint(const Vector3& v) { return CGALPoint(v.x(),v.y(),v.z()); }

template<class CGALPoint>
inline Vector3 toVector3(const CGALPoint& v) { return Vector3(v.x(),v.y(),v.z()); }

#endif

#include <stack>
#include <iostream>


IndexArrayPtr 
PGL::delaunay_point_connection(const Point3ArrayPtr points)
{
#ifdef WITH_CGAL
    Triangulation triangulation;
    uint32_t pointCount = 0;
    for (Point3Array::const_iterator it = points->begin(); it != points->end(); ++it)
        triangulation.insert(toPoint<TPoint>(*it))->info() = pointCount++;


    IndexArrayPtr result(new IndexArray(points->size(),Index()));
    for(Triangulation::Finite_edges_iterator it = triangulation.finite_edges_begin();
        it != triangulation.finite_edges_end(); ++it){
            uint32_t source = it->first->vertex(it->second)->info();
            uint32_t target = it->first->vertex(it->third)->info();
            result->getAt(source).push_back(target);
            result->getAt(target).push_back(source);
    } 
#else
    IndexArrayPtr result;
#endif
    return result;
}


struct PointSorter {
    PointSorter(Point3ArrayPtr _points, uint32_t _ref) : points(_points), refpoint(_points->getAt(_ref)) {}
    Point3ArrayPtr points;
    const Vector3& refpoint;

    bool operator()(uint32_t i, uint32_t j){
        return norm(refpoint-points->getAt(i)) <  norm(refpoint-points->getAt(j));  
    }
};

 IndexArrayPtr 
PGL::k_closest_points_from_delaunay(const Point3ArrayPtr points, size_t k)
{
    IndexArrayPtr res = delaunay_point_connection(points);
    IndexArrayPtr filteredres(new IndexArray(res->size()));
    IndexArray::iterator ittarget = filteredres->begin();
    uint32_t pointid = 0;
    for (IndexArray::iterator it = res->begin(); it != res->end(); ++it, ++ittarget){
        if (it->size() <= k) *ittarget = *it;
        else {
            std::sort(it->begin(), it->end(),PointSorter(points,pointid));
            *ittarget = Index(it->begin(),it->begin()+k);
        }
        ++pointid;
    }
    return filteredres;
}


#include <plantgl/algo/grid/kdtree.h>

IndexArrayPtr 
PGL::k_closest_points_from_ann(const Point3ArrayPtr points, size_t k, bool symmetric)
{
#ifdef WITH_ANN
    ANNKDTree3 kdtree(points);
    IndexArrayPtr result = kdtree.k_nearest_neighbors(k);
    if(symmetric) result = symmetrize_connections(result);
    return result;
#else
    return IndexArrayPtr();
#endif
}

#ifdef WITH_CGAL
#include <CGAL/Cartesian.h>

typedef CGAL::Cartesian<double>   CK;
typedef CK::Point_3               CPoint;
typedef boost::tuple<CPoint,int>  CPoint_and_int;
#endif
/*
//definition of the property map
struct My_point_property_map{
  typedef CPoint value_type;
  typedef const value_type& reference;
  typedef const CPoint_and_int& key_type;
  typedef boost::readable_property_map_tag category;
};

//get function for the property map
My_point_property_map::reference 
get(My_point_property_map,My_point_property_map::key_type p)
{return boost::get<0>(p);}

#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Search_traits_adapter.h>

typedef CGAL::Search_traits_3<CK>                                                   Traits_base;
typedef CGAL::Search_traits_adapter<CPoint_and_int,My_point_property_map,Traits_base>    Traits;


typedef CGAL::Orthogonal_k_neighbor_search<Traits>                      K_neighbor_search;
typedef K_neighbor_search::Tree                                         Tree;
typedef K_neighbor_search::Distance                                     Distance;


// typedef CGAL::Search_traits_3<K> TreeTraits;
// typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
// typedef Neighbor_search::Tree Tree;

#endif

IndexArrayPtr 
PGL::k_closest_points_from_cgal(const Point3ArrayPtr points, size_t k)
{
    size_t nbPoints = points->size();
    IndexArrayPtr result(new IndexArray(nbPoints,Index(0)));

    std::list<CPoint_and_int> pointdata;
    size_t pid = 0;
	for (Point3Array::const_iterator it = points->begin(); it != points->end() ; ++it, ++pid)
		pointdata.push_back(CPoint_and_int(toPoint<CPoint>(*it),pid));

   Tree tree(pointdata.begin(), pointdata.end());

   std::list<CPoint_and_int>::const_iterator piter = pointdata.begin();
   for(pid = 0; pid < nbPoints && piter != pointdata.end(); ++pid, ++piter){
       K_neighbor_search search(tree, piter->get<0>(), k+1);
       Index& cindex = result->getAt(pid);
       for(K_neighbor_search::iterator it = search.begin(); it != search.end(); ++it){
           size_t ngpid = it->first.get<1>();
           if (ngpid != pid) cindex.push_back(ngpid);
        }
   }


    return result;
}
*/

IndexArrayPtr 
PGL::symmetrize_connections(const IndexArrayPtr adjacencies)
{
    IndexArrayPtr newadjacencies(new IndexArray(*adjacencies));
    size_t pid = 0;
    for(IndexArray::const_iterator itpc = newadjacencies->begin(); itpc != newadjacencies->end(); ++itpc, ++pid)
    {
        for(Index::const_iterator itc = itpc->begin(); itc != itpc->end(); ++itc)
        {
            Index& target = newadjacencies->getAt(*itc);
            if(find(target.begin(),target.end(),pid) == target.end())
                target.push_back(pid);
        }
    }

    return newadjacencies;
}

#include "dijkstra.h"
#include <plantgl/tool/util_hashset.h>
#include <plantgl/tool/util_hashmap.h>
#include <list>

struct OneDistance {
        inline real_t operator()(uint32_t a, uint32_t b) const { return 1; }
        OneDistance() {}
};

ALGO_API IndexArrayPtr 
PGL::connect_all_connex_components(const Point3ArrayPtr points, const IndexArrayPtr adjacencies, bool verbose)
{
#ifdef WITH_ANN
    // ids of points not accessible from the root connex component
    pgl_hash_set_uint32 nonconnected;

    size_t nbtotalpoints = points->size();

    // connected points. they are not inserted in the same order than points
    Point3ArrayPtr refpoints(new Point3Array());
    // nb of connected points. store to not recompute each time
    uint32_t nbrefpoints = 0;
    // map of point id from 'refpoints' to 'points' structure
    pgl_hash_map<uint32_t,uint32_t> pidmap;

    // connections to add to connect all connex components
    std::list<std::pair<uint32_t,uint32_t> > addedconnections;
    // root to consider for next connex component
    uint32_t next_root = 0;

    OneDistance distcomputer;
    while (true){

        // find all points accessible from next_root
        std::pair<Uint32Array1Ptr,RealArrayPtr> root_connections = dijkstra_shortest_paths(adjacencies,next_root,distcomputer);
        Uint32Array1Ptr parents = root_connections.first;

        // update set of refpoints (point connected) and non connnected.
        if(next_root == 0) { // if it is the first round, add nonconnected
            uint32_t pid = 0;
            for (Uint32Array1::const_iterator itParent = parents->begin(); itParent != parents->end(); ++itParent, ++pid)
            {
                if (*itParent == UINT32_MAX) {
                    nonconnected.insert(pid);
                }
                else {
                    refpoints->push_back(points->getAt(pid));
                    pidmap[nbrefpoints] = pid;
                    ++nbrefpoints;
                }
            }
        }
        else
        {   // iter on nonconnected and remove a point from this set if it has a parent.
            std::list<uint32_t> toerase;
            for(pgl_hash_set_uint32::const_iterator itncpid = nonconnected.begin();
                itncpid != nonconnected.end(); ++itncpid)
            {
                if(parents->getAt(*itncpid) != UINT32_MAX)
                {
                    uint32_t pid = *itncpid;
                    refpoints->push_back(points->getAt(pid));
                    pidmap[nbrefpoints] = pid;
                    ++nbrefpoints;
                    toerase.push_back(pid);
                }
            }
            for(std::list<uint32_t>::const_iterator iterase = toerase.begin(); iterase != toerase.end(); ++iterase)
                nonconnected.erase(*iterase);
        }
        if (verbose) printf("\x0dNb points processed %i (%.2f%%) [left : %i (%.2f%%)].\n",nbrefpoints,100 * nbrefpoints /float(nbtotalpoints), nonconnected.size(), 100 * nonconnected.size()/float(nbtotalpoints),next_root);
        // if no more non connected point, break
        if (nonconnected.empty()) break;

        // create kdtree from connected points
        ANNKDTree3 kdtree(refpoints);
        real_t dist = REAL_MAX;
        std::pair<uint32_t,uint32_t> connection;
        bool allempty = true;

        // find shortest connections between a non connected point and a connected one
        for(pgl_hash_set_uint32::const_iterator itncpid = nonconnected.begin(); itncpid != nonconnected.end(); ++itncpid){
            Index cp = kdtree.k_closest_points(points->getAt(*itncpid),1,dist);
            if (!cp.empty()){
                uint32_t refpoint = pidmap[cp[0]];
                real_t newdist = norm(points->getAt(refpoint)-points->getAt(*itncpid));
                if (newdist < dist ){
                    connection = std::pair<uint32_t,uint32_t>(refpoint,*itncpid);
                    dist = newdist;
                }
                allempty = false;
            }
        }
        assert(!allempty);
        // add the found connections into addedconnections and consider non connected point as next_root
        addedconnections.push_back(connection);
        next_root = connection.second;               
    }

    // copy adjacencies and update it with addedconnections
    IndexArrayPtr newadjacencies(new IndexArray(*adjacencies));
    for(std::list<std::pair<uint32_t,uint32_t> >::const_iterator itc = addedconnections.begin();
            itc != addedconnections.end(); ++itc)
    {
            newadjacencies->getAt(itc->first).push_back(itc->second);
            newadjacencies->getAt(itc->second).push_back(itc->first);
    }

    return newadjacencies;
#else
    return adjacencies;
#endif
}



struct PointDistance {
        const Point3ArrayPtr points;
        real_t operator()(uint32_t a, uint32_t b) const { return norm(points->getAt(a)-points->getAt(b)); }
        PointDistance(const Point3ArrayPtr _points) : points(_points) {}
};

Index
PGL::r_neighborhood(uint32_t pid, const Point3ArrayPtr points, const IndexArrayPtr adjacencies, const real_t radius)
{
    GEOM_ASSERT(points->size() == adjacencies->size());

    struct PointDistance pdevaluator(points);

    NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,pid,pdevaluator,radius);
    Index result;
    for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
          result.push_back(itn->id);
    return result;
}

IndexArrayPtr 
PGL::r_neighborhoods(const Point3ArrayPtr points, const IndexArrayPtr adjacencies, const RealArrayPtr radii)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());
    struct PointDistance pdevaluator(points);

    uint32_t current = 0;
    IndexArrayPtr result(new IndexArray(nbPoints));
    for(RealArray::const_iterator itradii = radii->begin(); itradii != radii->end(); ++itradii) {
        NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,current,pdevaluator,*itradii);
        Index lres;
        for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
            lres.push_back(itn->id);
        result->setAt(current,lres);
        current++;

    }
    return result;
}

IndexArrayPtr 
PGL::r_neighborhoods(const Point3ArrayPtr points, const IndexArrayPtr adjacencies, real_t radius, bool verbose)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());
    struct PointDistance pdevaluator(points);
    
    IndexArrayPtr result(new IndexArray(nbPoints));
    real_t cpercent = 0;
    for(uint32_t current = 0; current < nbPoints; ++current) {
        NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,current,pdevaluator,radius);
        Index lres;
        for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
            lres.push_back(itn->id);
        result->setAt(current,lres);
        if (verbose){
            real_t ncpercent = 100 * current / float(nbPoints);
            if(cpercent + 5 <= ncpercent ) {
                printf("density computed for %.2f%% of points.\n",ncpercent);
                cpercent = ncpercent;
            }
        }
    }
    return result;
}

struct PointAnisotropicDistance {
        const Point3ArrayPtr points;
	    Vector3 direction;
	    real_t alpha, beta;
        real_t operator()(uint32_t a, uint32_t b) const { return radialAnisotropicNorm(points->getAt(a)-points->getAt(b),direction,alpha,beta); }
        PointAnisotropicDistance(const Point3ArrayPtr _points, const Vector3& _direction, real_t _alpha, real_t _beta) 
		: points(_points), direction(_direction), alpha(_alpha), beta(_beta) {}
};


Index
PGL::r_anisotropic_neighborhood(uint32_t pid, const Point3ArrayPtr points, const IndexArrayPtr adjacencies, const real_t radius, 
					 const Vector3& direction, 
					 const real_t alpha, const real_t beta)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());

    struct PointAnisotropicDistance pdevaluator(points,direction,alpha,beta);

    NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,pid,pdevaluator,radius);
    Index result;
    for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
          result.push_back(itn->id);
    return result;
}

IndexArrayPtr 
PGL::r_anisotropic_neighborhoods(const Point3ArrayPtr points, const IndexArrayPtr adjacencies, const RealArrayPtr radii, 
			         const Point3ArrayPtr directions,
				 const real_t alpha, const real_t beta)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());
    GEOM_ASSERT(nbPoints == directions->size());

    uint32_t current = 0;
    IndexArrayPtr result(new IndexArray(nbPoints));

    for(RealArray::const_iterator itradii = radii->begin(); itradii != radii->end(); ++itradii) {
	struct PointAnisotropicDistance pdevaluator(points,directions->getAt(current),alpha,beta);
        NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,current,pdevaluator,*itradii);
        Index lres;
        for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
            lres.push_back(itn->id);
        result->setAt(current,lres);
        ++current;
    }
    return result;
}


IndexArrayPtr 
PGL::r_anisotropic_neighborhoods(const Point3ArrayPtr points, const IndexArrayPtr adjacencies, real_t radius, 
			                      const Point3ArrayPtr directions,
				                  const real_t alpha, const real_t beta)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());
    GEOM_ASSERT(nbPoints == directions->size());

    
    IndexArrayPtr result(new IndexArray(nbPoints));
    for(uint32_t current = 0; current < nbPoints; ++current) {
	    struct PointAnisotropicDistance pdevaluator(points,directions->getAt(current),alpha,beta);
        NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,current,pdevaluator,radius);
        Index lres;
        for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
            lres.push_back(itn->id);
        result->setAt(current,lres);
    }
    return result;
}


real_t
PGL::density_from_r_neighborhood(  uint32_t pid,
                               const Point3ArrayPtr points, 
			                   const IndexArrayPtr adjacencies, 
                               const real_t radius)
{
    Index r_neighborhood_value = r_neighborhood(pid, points, adjacencies, radius);
    return r_neighborhood_value.size()/ (radius * radius);
}


TOOLS(RealArrayPtr)
PGL::densities_from_r_neighborhood(const Point3ArrayPtr points, 
			                   const IndexArrayPtr adjacencies, 
                               const real_t radius)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());
    RealArrayPtr result(new RealArray(nbPoints));
    uint32_t current = 0;
    for(RealArray::iterator itres = result->begin(); itres != result->end(); ++itres){
        *itres = density_from_r_neighborhood(current, points,adjacencies,radius);
        ++current;
    }
    return result;
}

Index PGL::get_k_closest_from_n(const Index& adjacencies, const uint32_t k, uint32_t pid, const Point3ArrayPtr points)
{
        uint32_t nbnbg = adjacencies.size();
        if (nbnbg <= k) return adjacencies;
        RealArrayPtr distances(new RealArray(nbnbg));
        Vector3 self = points->getAt(pid);
        for(size_t pnid = 0; pnid < nbnbg; ++pnid)
                distances->setAt(pnid,norm(self-points->getAt(pnid)));
        Index sorted = get_sorted_element_order(distances);
        return Index(sorted.begin(),sorted.begin()+k);
}

Index PGL::k_neighborhood(uint32_t pid, const Point3ArrayPtr points, const IndexArrayPtr adjacencies, const uint32_t k)
{
    GEOM_ASSERT(points->size() == adjacencies->size());

    Index result;
    uint32_t nbnbg = adjacencies->getAt(pid).size();
    if (k <= nbnbg) {
        result = get_k_closest_from_n(adjacencies->getAt(pid),k,pid,points);
    }
    else {
        struct PointDistance pdevaluator(points);

        NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,pid,pdevaluator,REAL_MAX,k);
        for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
              result.push_back(itn->id);
    }
    return result;
}

IndexArrayPtr
PGL::k_neighborhoods(const Point3ArrayPtr points, const IndexArrayPtr adjacencies, const uint32_t k)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());
    struct PointDistance pdevaluator(points);
    
    IndexArrayPtr result(new IndexArray(nbPoints));
    for(uint32_t current = 0; current < nbPoints; ++current) {
        Index lres;
        const Index& candidates = adjacencies->getAt(current);
        /*if (k <= candidates.size()) {
            lres = get_k_closest_from_n(candidates,k,current,points);
        }
        else {*/
            NodeList lneighborhood = dijkstra_shortest_paths_in_a_range(adjacencies,current,pdevaluator,REAL_MAX,k);
            for(NodeList::const_iterator itn = lneighborhood.begin(); itn != lneighborhood.end(); ++itn)
                lres.push_back(itn->id);
        // }
        result->setAt(current,lres);
    }
    return result;
}


real_t
PGL::max_neighborhood_distance(  uint32_t pid,
                                        const Point3ArrayPtr points, 
			                            const Index& adjacency)
{
    real_t max_distance = 0;
    for(Index::const_iterator itad = adjacency.begin(); itad != adjacency.end(); ++itad)
        max_distance = std::max(max_distance,norm(points->getAt(pid)-points->getAt(*itad)));
    return max_distance;
}

real_t
PGL::density_from_k_neighborhood(  uint32_t pid,
                               const Point3ArrayPtr points, 
			                   const IndexArrayPtr adjacencies,
                               const uint32_t k)
{
    Index adjacency = (k == 0 ? adjacencies->getAt(pid) : k_neighborhood(pid, points, adjacencies, k ));
    real_t radius = max_neighborhood_distance(pid, points, adjacency);
    return adjacency.size() / (radius * radius);
}


TOOLS(RealArrayPtr)
PGL::densities_from_k_neighborhood(const Point3ArrayPtr points, 
			                   const IndexArrayPtr adjacencies,
                               const uint32_t k)
{
    uint32_t nbPoints = points->size();
    GEOM_ASSERT(nbPoints == adjacencies->size());
    GEOM_ASSERT(nbPoints == radii->size());
    RealArrayPtr result(new RealArray(nbPoints));
    uint32_t current = 0;
    real_t cpercent = 0;
    for(RealArray::iterator itres = result->begin(); itres != result->end(); ++itres){
        *itres = density_from_k_neighborhood(current, points,adjacencies,k);
        ++current;
        real_t ncpercent = 100 * current / float(nbPoints);
        if(cpercent + 5 <= ncpercent ) {
            printf("density computed for %.2f%% of points.\n",ncpercent);
            cpercent = ncpercent;

        }
    }
    return result;
}

#ifdef WITH_CGAL
#include <CGAL/linear_least_squares_fitting_3.h>

typedef CK::Line_3                CLine;
#endif

Vector3 PGL::pointset_orientation(const Point3ArrayPtr points, const Index& group )
{
#ifdef WITH_CGAL
	std::list<CPoint> pointdata;
	// for (Point3Array::const_iterator it = points->begin(); it != points->end() ; ++it)
	for (Index::const_iterator it = group.begin(); it != group.end() ; ++it)
		pointdata.push_back(toPoint<CPoint>(points->getAt(*it)));

	CLine line;
	linear_least_squares_fitting_3(pointdata.begin(), pointdata.end(), line, CGAL::Dimension_tag<0>());

	return toVector3(line.to_vector());
#else
    return Vector3(0,0,0);
#endif
}

Point3ArrayPtr PGL::pointsets_orientations(const Point3ArrayPtr points, const IndexArrayPtr groups)
{
	size_t nbPoints = points->size();
	Point3ArrayPtr result(new Point3Array(nbPoints));

    IndexArray::const_iterator itNbrhd = groups->begin();
    for(Point3Array::iterator itRes = result->begin(); itRes != result->end(); ++itRes, ++itNbrhd)
        *itRes = pointset_orientation(points,*itNbrhd);

	return result;
}

#ifdef WITH_CGAL
#include <CGAL/Cartesian.h>
#include <CGAL/Monge_via_jet_fitting.h>

typedef CGAL::Cartesian<real_t>  Data_Kernel;
typedef Data_Kernel::Point_3     DPoint;
typedef CGAL::Monge_via_jet_fitting<Data_Kernel> My_Monge_via_jet_fitting;
typedef My_Monge_via_jet_fitting::Monge_form     My_Monge_form;

#endif


std::vector<std::pair<real_t, TOOLS(Vector3)> >
PGL::principal_curvatures(const Point3ArrayPtr points, uint32_t pid, const Index& group)
{
      std::vector<std::pair<real_t, TOOLS(Vector3)> > result;
#ifdef WITH_CGAL
#ifdef WITH_LAPACK
      size_t d_fitting = 4;
      size_t d_monge = 4;

      std::vector<DPoint> in_points;
      in_points.push_back(toPoint<DPoint>(points->getAt(pid)));

      for(Index::const_iterator itNg = group.begin(); itNg != group.end(); ++itNg)
          if (*itNg != pid) in_points.push_back(toPoint<DPoint>(points->getAt(*itNg)));

      My_Monge_form monge_form;
      My_Monge_via_jet_fitting monge_fit;
      monge_form = monge_fit(in_points.begin(), in_points.end(), d_fitting, d_monge);

      for (int i = 0 ; i < 3; ++i){
          std::pair<real_t, TOOLS(Vector3)> a;
          a.first = monge_fit.pca_basis(i).first;
          a.second = toVector3(monge_fit.pca_basis(i).second);
          result.push_back(a);
      }
#endif
#endif
      return result;

}

std::vector<std::vector<std::pair<real_t, TOOLS(Vector3)> > >
PGL::principal_curvatures(const Point3ArrayPtr points, const IndexArrayPtr groups)
{
    std::vector<std::vector<std::pair<real_t, TOOLS(Vector3)> > > result;
    uint32_t i = 0;
    for(IndexArray::const_iterator it = groups->begin(); it != groups->end(); ++it, ++i)
        result.push_back(principal_curvatures(points,i,*it));
    return result;
}

std::vector<std::vector<std::pair<real_t, TOOLS(Vector3)> > >
PGL::principal_curvatures(const Point3ArrayPtr points, const IndexArrayPtr adjacencies, real_t radius)
{
    std::vector<std::vector<std::pair<real_t, TOOLS(Vector3)> > > result;
    uint32_t nbPoints = points->size();

    for(uint32_t i = 0; i < nbPoints; ++i){
        Index ng = r_neighborhood(i,points, adjacencies, radius);
        result.push_back(principal_curvatures(points,i,ng));
    }
    return result;

}

RealArrayPtr
PGL::adaptive_radii( const RealArrayPtr density,
                 real_t minradius, real_t maxradius,
                 QuantisedFunctionPtr densityradiusmap)
{
    RealArrayPtr radii(new RealArray(density->size()));
    real_t mindensity = *density->getMin();
    real_t maxdensity = *density->getMax();
    real_t deltadensity = maxdensity-mindensity;
    real_t deltaradius = maxradius - minradius;
    RealArray::iterator itradius = radii->begin();
    for(RealArray::const_iterator itDensity = density->begin(); itDensity != density->end(); ++itDensity, ++itradius)
    {
        real_t normedDensity = (*itDensity-mindensity)/deltadensity;
        *itradius = minradius  + deltaradius * (densityradiusmap ? densityradiusmap->getValue(normedDensity) : normedDensity ) ; 
    }
    return radii;

}

Point3ArrayPtr
PGL::adaptive_contration(const Point3ArrayPtr points, 
                     const Point3ArrayPtr orientations,
                     const IndexArrayPtr adjacencies, 
                     const RealArrayPtr density,
                     real_t minradius, real_t maxradius,
                     QuantisedFunctionPtr densityradiusmap,
                     const real_t alpha, 
	                 const real_t beta)
{
    RealArrayPtr radii = adaptive_radii(density, minradius, maxradius, densityradiusmap);
    IndexArrayPtr neighborhoods = r_anisotropic_neighborhoods(points,  adjacencies, radii, orientations, alpha, beta);    
    return centroids_of_groups(points, neighborhoods);
}

std::pair<TOOLS(Uint32Array1Ptr),TOOLS(RealArrayPtr)>
PGL::points_dijkstra_shortest_path(const Point3ArrayPtr points, 
			         const IndexArrayPtr adjacencies, 
                     uint32_t root)
{
    struct PointDistance pdevaluator(points);
    return dijkstra_shortest_paths(adjacencies,root,pdevaluator);
}

struct DistanceCmp {
        const RealArrayPtr distances;
        real_t operator()(uint32_t a, uint32_t b) const { return distances->getAt(a) < distances->getAt(b); }
        DistanceCmp(const RealArrayPtr _distances) : distances(_distances){}
};



Index 
PGL::get_sorted_element_order(const TOOLS(RealArrayPtr) distances)
{
    Index result(range<Index>(distances->size(),0,1));
    std::sort(result.begin(),result.end(),DistanceCmp(distances));
    return result;
}

#include <plantgl/tool/util_hashset.h>

IndexArrayPtr 
PGL::quotient_points_from_adjacency_graph(  const real_t binsize,
                                const Point3ArrayPtr points, 
			                    const IndexArrayPtr adjacencies, 
			                    const TOOLS(RealArrayPtr) distance_to_root)
{
    uint32_t nbpoints = points->size();
    Index sortedpoints = get_sorted_element_order(distance_to_root);
    uint32_t nextlimit = 0;
    uint32_t currentlimit = nextlimit;
    real_t currentbinlimit = 0;
    real_t nextbinlimit = binsize;
    IndexArrayPtr groups(new IndexArray());

    while(nextlimit < nbpoints) {
        while(nextlimit < nbpoints && distance_to_root->getAt(sortedpoints[nextlimit]) < nextbinlimit) {
            ++nextlimit;
        }
        if (nextlimit == currentlimit) {
            if (nextlimit < nbpoints && distance_to_root->getAt(sortedpoints[nextlimit]) == REAL_MAX) break;
            else {
                nextbinlimit += binsize;
                continue;
            }
        }
        // printf("in [%i,%i]  (%i)\n",currentlimit,nextlimit,nbpoints);

        pgl_hash_set_uint32 pointmap;
        uint32_t nbgroupincluster = 0;
        for (Index::const_iterator itpoint = sortedpoints.begin()+currentlimit; itpoint != sortedpoints.begin()+nextlimit; ++itpoint)
            if(pointmap.find(*itpoint) == pointmap.end()){
                pointmap.insert(*itpoint);
                Index newgroup;
                newgroup.push_back(*itpoint);
                std::stack<Index> sn;
                sn.push(adjacencies->getAt(*itpoint));
                while(!sn.empty()){
                    Index curneighborhood = sn.top();
                    sn.pop();
                    for (Index::const_iterator itpoint2 = curneighborhood.begin(); itpoint2 != curneighborhood.end(); ++itpoint2){
                        real_t pdist = distance_to_root->getAt(*itpoint2);
                        if( currentbinlimit <= pdist && pdist < nextbinlimit && pointmap.find(*itpoint2) == pointmap.end()){
                            pointmap.insert(*itpoint2);
                            newgroup.push_back(*itpoint2);
                            sn.push(adjacencies->getAt(*itpoint2));
                        }
                    }
                }
                assert(newgroup.size() > 0);
                groups->push_back(newgroup);
                ++nbgroupincluster;
            }
        assert(nbgroupincluster > 0);
        currentlimit = nextlimit;
        currentbinlimit = nextbinlimit;
        nextbinlimit += binsize;
        if (currentlimit < nbpoints && distance_to_root->getAt(sortedpoints[currentlimit]) == REAL_MAX) break;
    }
    return groups;
}


IndexArrayPtr 
PGL::quotient_adjacency_graph(const IndexArrayPtr adjacencies, 
			                    const IndexArrayPtr groups)
{
    uint32_t nbpoints = adjacencies->size();
    uint32_t  * group = new uint32_t[nbpoints];
    for (uint32_t p = 0 ; p < nbpoints; ++p) group[p] = UINT32_MAX; // Default group is no group.
    uint32_t cgroup = 0;
    for(IndexArray::const_iterator itgs = groups->begin(); itgs != groups->end(); ++itgs, ++cgroup)
    {
        for(Index::const_iterator itg = itgs->begin(); itg != itgs->end(); ++itg){
            group[*itg] = cgroup;
        }
    }
    IndexArrayPtr macroadjacencies(new IndexArray(groups->size(),Index()));
    uint32_t cnode = 0;
    for(IndexArray::const_iterator itn = adjacencies->begin(); itn != adjacencies->end(); ++itn, ++cnode)
    {
        uint32_t cgroup = group[cnode];
        if (cgroup == UINT32_MAX) continue; // No group assigned to this point.
        Index& cmadjacency = macroadjacencies->getAt(cgroup);
        for(Index::const_iterator itadgroup = itn->begin(); itadgroup != itn->end(); ++itadgroup){
            uint32_t adjacentgroup = group[*itadgroup];
            assert (adjacentgroup != UINT32_MAX);
            if (cgroup != adjacentgroup){
                if(std::find(cmadjacency.begin(),cmadjacency.end(),adjacentgroup) == cmadjacency.end()){
                    cmadjacency.push_back(adjacentgroup);
                }
            }
            
        }
    }
    delete [] group;
    return macroadjacencies;
}

ALGO_API TOOLS(Vector3) 
PGL::centroid_of_group(const Point3ArrayPtr points, 
			           const Index& group)
{
        Vector3 gcentroid; real_t nbpoints = 0;
        for(Index::const_iterator itn = group.begin(); itn != group.end(); ++itn,++nbpoints){
            gcentroid += points->getAt(*itn);
        }
        return gcentroid/nbpoints;
}

Point3ArrayPtr 
PGL::centroids_of_groups(const Point3ArrayPtr points, 
			             const IndexArrayPtr groups)
{
    Point3ArrayPtr result(new Point3Array(groups->size()));
    uint32_t cgroup = 0;
    for(IndexArray::const_iterator itgs = groups->begin(); itgs != groups->end(); ++itgs, ++cgroup)
    {
        result->setAt(cgroup,centroid_of_group(points,*itgs));
    }
    return result;
}

Point3ArrayPtr
PGL::skeleton_from_distance_to_root_clusters(const Point3ArrayPtr points, uint32_t root, real_t binsize, uint32_t k, 
                                             TOOLS(Uint32Array1Ptr)& group_parents, IndexArrayPtr& group_components, 
                                             bool connect_all_points, bool verbose)
{
    if(verbose)std::cout << "Compute Remanian graph." << std::endl;
#ifdef WITH_ANN
    IndexArrayPtr remaniangraph =  k_closest_points_from_ann(points, k, connect_all_points);
#else
    IndexArrayPtr remaniangraph =  k_closest_points_from_delaunay(points, k);
#endif
    if (connect_all_points){
        if(verbose)std::cout << "Connect all components of Riemanian graph." << std::endl;
        remaniangraph =  connect_all_connex_components(points, remaniangraph, verbose);
    }
    if(verbose)std::cout << "Compute distance to root." << std::endl;
    std::pair<TOOLS(Uint32Array1Ptr),TOOLS(RealArrayPtr)> shortest_pathes = points_dijkstra_shortest_path(points, remaniangraph, root);
    Uint32Array1Ptr parents = shortest_pathes.first;
    RealArrayPtr distances_to_root = shortest_pathes.second;
    if(verbose)std::cout << "Compute cluster according to distance to root." << std::endl;
    group_components =  quotient_points_from_adjacency_graph(binsize, points, remaniangraph, distances_to_root);
    std::cout << "Nb of groups : " << group_components->size() << std::endl;
    if(verbose)std::cout << "Compute adjacency graph of groups." << std::endl;
    IndexArrayPtr group_adjacencies = quotient_adjacency_graph(remaniangraph, group_components);
    if(verbose)std::cout << "Compute centroid of groups." << std::endl;
    Point3ArrayPtr group_centroid =  centroids_of_groups(points,group_components);
    if(verbose)std::cout << "Compute spanning tree of groups." << std::endl;
    shortest_pathes = points_dijkstra_shortest_path(group_centroid, group_adjacencies, 0);
    group_parents = shortest_pathes.first;
    return group_centroid;
}

// Livny stuff

// compute parent-children relation from child-parent relation
IndexArrayPtr PGL::determine_children(const Uint32Array1Ptr parents, uint32_t& root)
{
    IndexArrayPtr result(new IndexArray(parents->size(),Index()));
    uint32_t pid = 0;
    for(Uint32Array1::const_iterator it = parents->begin(); it != parents->end(); ++it, ++pid){
         if (pid == *it) root = pid;
         else result->getAt(*it).push_back(pid);
    }
    return result;
}

#include <plantgl/scenegraph/container/indexarray_iterator.h>

RealArrayPtr PGL::carried_length(const Point3ArrayPtr points, const Uint32Array1Ptr parents)
{
    RealArrayPtr result(new RealArray(points->size()));
    uint32_t root;
    IndexArrayPtr children = determine_children(parents,root);


    IndexArrayPostOrderConstIterator piterator(children, root);

    // look to the node in post order and for each node compute its weigth as sum of children weigth + length of segment to children.
    for(;!piterator.atEnd(); ++piterator)
    {
        Index& nextgroup = children->getAt(*piterator);
        real_t sumw = 0;
        Vector3& refpoint = points->getAt(*piterator);
        for (Index::const_iterator itchildren = nextgroup.begin(); itchildren != nextgroup.end(); ++itchildren)
            sumw += result->getAt(*itchildren) + norm(refpoint-points->getAt(*itchildren));
        result->setAt(*piterator,sumw);
    }

    return result;
}



Point3ArrayPtr PGL::optimize_orientations(const Point3ArrayPtr points, 
                                          const Uint32Array1Ptr parents, 
                                          const RealArrayPtr weights)
{
    uint32_t nbPoints = points->size();
    Point3ArrayPtr result( new Point3Array(nbPoints));
    uint32_t root;
    IndexArrayPtr children = determine_children(parents, root);

    // Compute first orientation as average direction to children weighted with 'weights'
    Vector3 firstorientation;
    real_t sumw = 0.0;
    for(Index::const_iterator itRChildren = children->getAt(root).begin(); 
        itRChildren != children->getAt(root).end(); ++itRChildren){
          real_t cv = weights->getAt(*itRChildren);
          firstorientation += cv * direction(points->getAt(*itRChildren)-points->getAt(root));
          sumw += cv;

    }
    result->setAt(root,firstorientation / sumw);

    // compute direction of all points as compromive between orientation of parent and direction(node-parent)
    IndexArrayPreOrderConstIterator piterator(children,root);
    for(++piterator; !piterator.atEnd(); ++piterator){
        uint32_t pid = *piterator;
        real_t cv = weights->getAt(pid);
        const Vector3& cpos = points->getAt(pid);
        uint32_t parent = parents->getAt(pid);
        real_t cvp_cv = (weights->getAt(parent) + cv) / 2;
        const Vector3& ppos = points->getAt(parent);
        Vector3 dp = direction(cpos-ppos);
        // assert (weights->getAt(parent) > cv);
        Vector3 ov = (cvp_cv* result->getAt(parent) + cv * dp) / (cvp_cv + cv);
        assert (ov.isValid());
        result->getAt(pid) = direction(ov);
    }
    return result;

}

Point3ArrayPtr PGL::optimize_positions(const Point3ArrayPtr points, 
                                           const Point3ArrayPtr orientations, 
                                           const TOOLS(Uint32Array1Ptr) parents, 
                                           const TOOLS(RealArrayPtr) weights)
{
    uint32_t nbPoints = points->size();
    Point3ArrayPtr result( new Point3Array(nbPoints));
    uint32_t root;
    IndexArrayPtr children = determine_children(parents, root);
    result->setAt(root,points->getAt(root));

    // compute position of all points as compromive between orientation and previous position
    IndexArrayPreOrderConstIterator piterator(children,root);
    for(++piterator; !piterator.atEnd(); ++piterator){
        uint32_t pid = *piterator;
        real_t cv = weights->getAt(pid);
        const Vector3& cpos = points->getAt(pid);

        uint32_t parent = parents->getAt(pid);
        real_t cvp_cv = (weights->getAt(parent) + cv)/2;
        const Vector3& ppos = points->getAt(parent);

        Vector3 relpos = cpos - ppos;
        real_t pdist = norm(relpos);

        const Vector3& corient = orientations->getAt(pid);
        const Vector3& porient = orientations->getAt(parent);

        // this position comes from the constraint of having edges coherent with orientation
        Vector3 relidealpos = result->getAt(parent) +  direction(corient+porient)*pdist;

        // this position comes from the constraint of having center of edges preserved
        Vector3 relt2pos = cpos+ppos-result->getAt(parent);

        // computes the new position from the compromise of the two previous positions
        Vector3 newpos = ( relt2pos* cv  + relidealpos * cvp_cv ) / (cv + cvp_cv);
        result->setAt(pid,newpos);
    }

    return result;
}

#include <plantgl/scenegraph/geometry/lineicmodel.h>

real_t 
PGL::average_radius(const Point3ArrayPtr points, 
                      const Point3ArrayPtr nodes,
                      const TOOLS(Uint32Array1Ptr) parents,
                      uint32_t maxclosestnode)
{
#ifdef WITH_ANN
    uint32_t root;
    IndexArrayPtr children = determine_children(parents, root);

    real_t sum_min_dist = 0;
    uint32_t nb_samples = 0;
    ANNKDTree3 tree(nodes);
    for (Point3Array::const_iterator itp = points->begin(); itp != points->end(); ++itp)
    {
        real_t minpdist = REAL_MAX;
        Index nids = tree.k_closest_points(*itp,10);
        for(Index::const_iterator nit = nids.begin(); nit != nids.end(); ++nit){
            Vector3 point(*itp);
            real_t d = closestPointToSegment(point,nodes->getAt(*nit),nodes->getAt(parents->getAt(*nit)));
            if (d < minpdist) minpdist = d;
            for (Index::const_iterator nitc = children->getAt(*nit).begin(); nitc != children->getAt(*nit).end(); ++nitc){
                Vector3 mpoint(*itp);
                d = closestPointToSegment(mpoint,nodes->getAt(*nit),nodes->getAt(parents->getAt(*nit)));
                if (d < minpdist) minpdist = d;
            }
        }
        if (minpdist != REAL_MAX) {
            sum_min_dist += minpdist;
            nb_samples += 1;
        }
    }
    return sum_min_dist / nb_samples;
#else
    return 0;
#endif
}

TOOLS(RealArrayPtr) 
PGL::estimate_radii(const Point3ArrayPtr nodes,
                    const TOOLS(Uint32Array1Ptr) parents, 
                    const TOOLS(RealArrayPtr) weights,
                    real_t averageradius,
                    real_t pipeexponent)
{
    uint32_t nbNodes = nodes->size();
    RealArrayPtr result( new RealArray(nbNodes,0));

    uint32_t root;
    IndexArrayPtr children = determine_children(parents, root);

    result->setAt(root,1);
    real_t cr = weights->getAt(root);
    real_t totalcu = cr;
    real_t totalcu_cr = cr;

    // compute position of all points as compromive between orientation and previous position
    IndexArrayPreOrderConstIterator piterator(children,root);
    for(++piterator; !piterator.atEnd(); ++piterator){
        real_t cu = weights->getAt(*piterator);
        totalcu += cu;
        totalcu_cr += cu * pow(cu/cr,pipeexponent);
    }

    real_t radiusroot = totalcu * averageradius / totalcu_cr;
    // real_t radiusroot = pow(totalcu * averageradius / totalcu_cr,1.0/pipeexponent);

    uint32_t nid = 0;
    for(RealArray::iterator itres = result->begin(); itres != result->end(); ++itres, ++nid)
        *itres = radiusroot * weights->getAt(nid) / cr;

    return result;
}

/*
bool intersection_test2(const Vector3& root, real_t rootradius, 
                       const Vector3& p1, real_t radius1, 
                       const Vector3& p2, real_t radius2,
                       real_t overlapfilter)
{
    Vector3 p1c = p1-root;
    Vector3 p2c = p2-root;
    Vector3 cmpplannormal = direction(cross(p1c,p2c));
    real_t diffangle = angle(p1c,p2c,cmpplannormal);
    Vector3 v1 = direction(cross(p1c,cmpplannormal));
    Vector3 v2 = direction(cross(p2c,cmpplannormal));
    Vector3 p1a = v1*rootradius;
    Vector3 p2a = v2*rootradius;
    Vector3 p1b = p1c + v1*radius1;
    Vector3 p2b = p2c + v2*radius2;
    Ray r(Vector3::ORIGIN,direction(p2a));
    if(r.intersect(p1a,p1b)){
    }

}
*/

bool intersection_test(const Vector3& root, real_t rootradius, 
                       const Vector3& p1, real_t radius1, 
                       const Vector3& p2, real_t radius2,
                       real_t overlapfilter)
{
    // We check if mid central point of cylinder 2 is in cylinder 1
    Vector3 midp1 = (p1-root) * overlapfilter;
    Vector3 rp2 = p2-root;
    real_t normp2 = norm(rp2);
    Vector3 drp2 = rp2/normp2;
    real_t corespu = dot(midp1,drp2);
    real_t corespt = corespu / normp2;
    Vector3 crp2 = drp2 * corespu;
    real_t radatcrp2 = rootradius + (radius2 -  rootradius) * corespt;
    return norm(midp1-crp2) < radatcrp2;

}


ALGO_API Index PGL::filter_short_nodes(const Point3ArrayPtr nodes,
                            const TOOLS(Uint32Array1Ptr) parents, 
                            const TOOLS(RealArrayPtr) radii,
                            real_t edgelengthfilter,
                            real_t overlapfilter)
{
    Index toremove;

    uint32_t root;
    IndexArrayPtr children = determine_children(parents, root);

    IndexArrayPostOrderConstIterator piterator(children,root);
    for(; !piterator.atEnd(); ++piterator){
        // printf("vid : %i\n",*piterator);
        const Index& pchildren = children->getAt(*piterator);
        const Vector3& rpos = nodes->getAt(*piterator);
        Index fpchildren;
        for (Index::const_iterator itch = pchildren.begin(); itch != pchildren.end();  ++itch)
        {
            if (norm(rpos-nodes->getAt(*itch)) < edgelengthfilter) toremove.push_back(*itch);
            else fpchildren.push_back(*itch);
        }
        switch (fpchildren.size()){
        case 0:
            break;
        case 1:
            if (*piterator != root)
                if (norm(direction(rpos-nodes->getAt(parents->getAt(*piterator)))- direction(nodes->getAt(fpchildren[0])-rpos)) < 0.1){
                    printf("continuity\n");
                    toremove.push_back(*piterator);
                }
            break;
        default:
            for (Index::const_iterator itch = fpchildren.begin(); itch != fpchildren.end();  ++itch)
                for (Index::const_iterator itch2 = itch+1; itch2 != fpchildren.end();  ++itch2)
                {
                    if (intersection_test(rpos, radii->getAt(*piterator),
                                               nodes->getAt(*itch), radii->getAt(*itch),
                                               nodes->getAt(*itch2), radii->getAt(*itch2),
                                               overlapfilter))
                    {
                        toremove.push_back(*itch2);
                    }
                    else if (intersection_test(rpos, radii->getAt(*piterator),
                                               nodes->getAt(*itch2), radii->getAt(*itch2),
                                               nodes->getAt(*itch), radii->getAt(*itch),
                                               overlapfilter))
                    {
                        toremove.push_back(*itch);
                    }
                }
            break;
        }
    }
    return toremove;
}

void PGL::remove_nodes(const Index& toremove,
                       Point3ArrayPtr& nodes,
                       TOOLS(Uint32Array1Ptr)& parents, 
                       TOOLS(RealArrayPtr)& radii)
{
    Index sortedtoremove(toremove);
    std::sort(sortedtoremove.begin(),sortedtoremove.end());
    uint32_t nbNodes = nodes->size();
    uint32_t *  idmap = new uint32_t[nbNodes];
    uint32_t noid = UINT32_MAX;

    uint32_t pid = 0;
    uint32_t * itid = idmap;
    Index::const_iterator itdel = sortedtoremove.begin();
    for (uint32_t cid = 0;  cid < nbNodes; ++cid, ++itid){
        if (itdel != sortedtoremove.end() && cid == *itdel) { 
                *itid = noid; 
                while(itdel != sortedtoremove.end() && *itdel <= cid) ++itdel; 
        }
        else { 
            *itid = pid; 
            ++pid; 
        }
    }
    uint32_t nbNewNodes = pid;

    Point3ArrayPtr newnodes(new Point3Array(*nodes));
    RealArrayPtr newradii(new RealArray(*radii));
    uint32_t lastdelid = UINT32_MAX;
    for(Index::const_reverse_iterator itdel2 = sortedtoremove.rbegin(); itdel2 != sortedtoremove.rend(); ++itdel2){
        if (*itdel2 != lastdelid){
            newnodes->erase(newnodes->begin()+*itdel2);
            newradii->erase(newradii->begin()+*itdel2);
            lastdelid = *itdel2;
        }
    }

    Uint32Array1Ptr newparents(new Uint32Array1(nbNewNodes));
    pid = 0;
    for(Uint32Array1::iterator itParent = newparents->begin(); itParent != newparents->end(); ++itParent)
    {
            ++pid; while (idmap[pid] == noid) ++pid;
            uint32_t parent = parents->getAt(pid);
            while(idmap[parent] == noid) parent = parents->getAt(parent);
            *itParent = idmap[parent];
    }

    delete [] idmap;

    nodes = newnodes;
    radii = newradii;
    parents = newparents;

}