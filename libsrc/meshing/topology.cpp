#include <mystdlib.h>
#include "meshing.hpp"

namespace netgen
{
  template <class T>
  void QuickSortRec (FlatArray<T> & data,
		     int left, int right)
  {
    int i = left;
    int j = right;
    T midval = data[(left+right)/2];
  
    do
      {
	while (data[i] < midval) i++;
	while (midval < data[j]) j--;
      
	if (i <= j)
	  {
	    Swap (data[i], data[j]);
	    i++; j--;
	  }
      }
    while (i <= j);
    if (left < j) QuickSortRec (data, left, j);
    if (i < right) QuickSortRec (data, i, right);
  }

  template <class T>
  void QuickSort (FlatArray<T> & data)
  {
    if (data.Size() > 1)
      QuickSortRec (data, 0, data.Size()-1);
  }






  MeshTopology ::  MeshTopology (const Mesh & amesh)
    : mesh(amesh)
  {
    buildedges = 1;
    buildfaces = 1;
    vert2element = 0;
    vert2surfelement = 0;
    vert2segment = 0;
    timestamp = -1;
  }

  MeshTopology :: ~MeshTopology ()
  {
    delete vert2element;
    delete vert2surfelement;
    delete vert2segment;
  }

  void MeshTopology :: Update()
  {
    static int timer = NgProfiler::CreateTimer ("topology");
    NgProfiler::RegionTimer reg (timer);

#ifdef PARALLEL
    // ParallelMeshTopology & paralleltop = mesh.GetParallelTopology();
#endif

  
    if (timestamp > mesh.GetTimeStamp()) return;
  
    int ne = mesh.GetNE();
    int nse = mesh.GetNSE();
    int nseg = mesh.GetNSeg();
    int np = mesh.GetNP();
    int nv = mesh.GetNV(); 
    int nfa = 0;
    int ned = edge2vert.Size();

    if (id == 0)
      PrintMessage (3, "Update mesh topology");

    (*testout) << " UPDATE MESH TOPOLOGY " << endl; 
    (*testout) << "ne   = " << ne << endl;
    (*testout) << "nse  = " << nse << endl;
    (*testout) << "nseg = " << nseg << endl;
    (*testout) << "np   = " << np << endl;
    (*testout) << "nv   = " << nv << endl;
  
    delete vert2element;
    delete vert2surfelement;
    delete vert2segment;
  
    Array<int,PointIndex::BASE> cnt(nv);
    Array<int> vnums;

    /*
      generate:
      vertex to element 
      vertex to surface element
      vertex to segment 
    */


    cnt = 0;
    for (ElementIndex ei = 0; ei < ne; ei++)
      {
	const Element & el = mesh[ei];
	for (int j = 0; j < el.GetNV(); j++)
	  cnt[el[j]]++;
      }

    vert2element = new TABLE<ElementIndex,PointIndex::BASE> (cnt);
    for (ElementIndex ei = 0; ei < ne; ei++)
      {
	const Element & el = mesh[ei];
	for (int j = 0; j < el.GetNV(); j++)
	  vert2element->AddSave (el[j], ei);
      }

    cnt = 0;
    for (SurfaceElementIndex sei = 0; sei < nse; sei++)
      {
	const Element2d & el = mesh[sei];
	for (int j = 0; j < el.GetNV(); j++)
	  cnt[el[j]]++;
      }

    vert2surfelement = new TABLE<int,PointIndex::BASE> (cnt);
    for (SurfaceElementIndex sei = 0; sei < nse; sei++)
      {
	const Element2d & el = mesh[sei];
	for (int j = 0; j < el.GetNV(); j++)
	  vert2surfelement->AddSave (el[j], sei+1);
      }

    cnt = 0;
    for (int i = 1; i <= nseg; i++)
      {
	const Segment & seg = mesh.LineSegment(i);
	cnt[seg[0]]++;
	cnt[seg[1]]++;
      }
 
    vert2segment = new TABLE<int,PointIndex::BASE> (cnt);
    for (int i = 1; i <= nseg; i++)
      {
	const Segment & seg = mesh.LineSegment(i);
	vert2segment->AddSave (seg[0], i);
	vert2segment->AddSave (seg[1], i);
      }

    if (buildedges)
      {
	static int timer1 = NgProfiler::CreateTimer ("topology::buildedges");
	NgProfiler::RegionTimer reg1 (timer1);
	
	if (id == 0)
	  PrintMessage (5, "Update edges ");
      
	edges.SetSize(ne);
	surfedges.SetSize(nse); 
	segedges.SetSize(nseg);

	for (int i = 0; i < ne; i++)
	  for (int j = 0; j < 12; j++)
	    edges[i][j].nr = -1;
	for (int i = 0; i < nse; i++)
	  for (int j = 0; j < 4; j++)
	    surfedges[i][j].nr = -1;

	// keep existing edges
	cnt = 0;
	for (int i = 0; i < edge2vert.Size(); i++)
	  cnt[edge2vert[i][0]]++;
	TABLE<int,PointIndex::BASE> vert2edge (cnt);
	for (int i = 0; i < edge2vert.Size(); i++)
	  vert2edge.AddSave (edge2vert[i][0], i+1);

	// ensure all coarse grid and intermediate level edges
	cnt = 0;
	for (int i = mesh.mlbetweennodes.Begin(); i < mesh.mlbetweennodes.End(); i++)
	  {
	    INDEX_2 parents = Sort (mesh.mlbetweennodes[i]);
	    if (parents[0] >= PointIndex::BASE) cnt[parents[0]]++;
	  }
	TABLE<int,PointIndex::BASE> vert2vertcoarse (cnt);
	for (int i = mesh.mlbetweennodes.Begin(); i < mesh.mlbetweennodes.End(); i++)
	  {
	    INDEX_2 parents = Sort (mesh.mlbetweennodes[i]);
	    if (parents[0] > PointIndex::BASE) vert2vertcoarse.AddSave (parents[0], parents[1]);
	  }

	Array<int,PointIndex::BASE> edgenr(nv);
	Array<int,PointIndex::BASE> edgeflag(nv);
	Array<int> vertex2;

	edgeflag = PointIndex::BASE-1;

	ned = edge2vert.Size();

	for (int i = PointIndex::BASE; i < nv+PointIndex::BASE; i++)
	  {
	    vertex2.SetSize (0);

	    for (int j = 0; j < vert2edge[i].Size(); j++)
	      {
		int ednr = vert2edge[i][j];
		int i2 = edge2vert.Get(ednr)[1];
		edgeflag[i2] = i;
		edgenr[i2] = ednr;
	      }

	    for (int j = 0; j < vert2vertcoarse[i].Size(); j++)    
	      {
		int v2 = vert2vertcoarse[i][j];
		if (edgeflag[v2] < i)
		  {
		    edgeflag[v2] = i;
		    vertex2.Append (v2);
		  }
	      }

	    FlatArray<ElementIndex> v2els = (*vert2element)[i];
	    for (int j = 0; j < v2els.Size(); j++)
	      {
		const Element & el = mesh[v2els[j]];
		int neledges = GetNEdges (el.GetType());
		const ELEMENT_EDGE * eledges = GetEdges0 (el.GetType());
		for (int k = 0; k < neledges; k++)
		  {
		    INDEX_2 edge(el[eledges[k][0]], el[eledges[k][1]]);
		    edge.Sort();
		    if (edge.I1() != i) continue;
		    
		    if (edgeflag[edge.I2()] < i)
		      {
			vertex2.Append (edge.I2());
			edgeflag[edge.I2()] = i;
		      }
		  }
	      }

	    for (int j = 0; j < (*vert2surfelement)[i].Size(); j++)
	      {
		int elnr = (*vert2surfelement)[i][j];
		const Element2d & el = mesh.SurfaceElement (elnr);

		int neledges = GetNEdges (el.GetType());
		const ELEMENT_EDGE * eledges = GetEdges0 (el.GetType());
	  
		for (int k = 0; k < neledges; k++)
		  {
		    INDEX_2 edge(el[eledges[k][0]], el[eledges[k][1]]);
		    edge.Sort();
		    if (edge.I1() != i) continue;
	     
		    if (edgeflag[edge.I2()] < i)
		      {
			vertex2.Append (edge.I2());
			edgeflag[edge.I2()] = i;
		      }
		  }
	      }

	    for (int j = 0; j < (*vert2segment)[i].Size(); j++)
	      {
		int elnr = (*vert2segment)[i][j];
		const Segment & el = mesh.LineSegment (elnr);

		INDEX_2 edge(el[0], el[1]);
		edge.Sort();
		if (edge.I1() != i) continue;
	     
		if (edgeflag[edge.I2()] < i)
		  {
		    vertex2.Append (edge.I2());
		    edgeflag[edge.I2()] = i;
		  }   
	      }

	    QuickSort (vertex2);
	    for (int j = 0; j < vertex2.Size(); j++)
	      {
		edgenr[vertex2[j]] = ++ned;
		edge2vert.Append (INDEX_2 (i, vertex2[j]));
	      }


	    for (int j = 0; j < (*vert2element)[i].Size(); j++)
	      {
		ElementIndex elnr = (*vert2element)[i][j];
		const Element & el = mesh[elnr];

		int neledges = GetNEdges (el.GetType());
		const ELEMENT_EDGE * eledges = GetEdges0 (el.GetType());
	  
		for (int k = 0; k < neledges; k++)
		  {
		    INDEX_2 edge(el[eledges[k][0]], el[eledges[k][1]]);
	      
		    int edgedir = (edge.I1() > edge.I2());
		    if (edgedir) swap (edge.I1(), edge.I2());
	     
		    if (edge.I1() != i) continue;
                    
		    int edgenum = edgenr[edge.I2()];
                    /*
		    if (edgedir) edgenum *= -1;
		    edges[elnr][k] = edgenum;
                    */
                    edges[elnr][k].nr = edgenum-1;
                    edges[elnr][k].orient = edgedir;
		  }
	      }

	    for (int j = 0; j < (*vert2surfelement)[i].Size(); j++)
	      {
		int elnr = (*vert2surfelement)[i][j];
		const Element2d & el = mesh.SurfaceElement (elnr);

		int neledges = GetNEdges (el.GetType());
		const ELEMENT_EDGE * eledges = GetEdges0 (el.GetType());
	  
		for (int k = 0; k < neledges; k++)
		  {
		    INDEX_2 edge(el[eledges[k][0]], el[eledges[k][1]]);
	      
		    int edgedir = (edge.I1() > edge.I2());
		    if (edgedir) swap (edge.I1(), edge.I2());
	     
		    if (edge.I1() != i) continue;

		    int edgenum = edgenr[edge.I2()];
		    // if (edgedir) edgenum *= -1;
		    // surfedges.Elem(elnr)[k] = edgenum;
                    surfedges.Elem(elnr)[k].nr = edgenum-1;
                    surfedges.Elem(elnr)[k].orient = edgedir;
                    
		  }
	      }

	    for (int j = 0; j < (*vert2segment)[i].Size(); j++)
	      {
		int elnr = (*vert2segment)[i][j];
		const Segment & el = mesh.LineSegment (elnr);

		INDEX_2 edge(el[0], el[1]);
	      
		int edgedir = (edge.I1() > edge.I2());
		if (edgedir) swap (edge.I1(), edge.I2());
	      
		if (edge.I1() != i) continue;

		int edgenum = edgenr[edge.I2()];
                /*
		if (edgedir) edgenum *= -1;
		segedges.Elem(elnr) = edgenum;
                */
                segedges.Elem(elnr).nr = edgenum-1;
                segedges.Elem(elnr).orient = edgedir;
	      }
	  }
      }



    // generate faces
    if (buildfaces) 
      {
	static int timer2 = NgProfiler::CreateTimer ("topology::buildfaces");
	static int timer2a = NgProfiler::CreateTimer ("topology::buildfacesa");
	static int timer2b = NgProfiler::CreateTimer ("topology::buildfacesb");
	static int timer2c = NgProfiler::CreateTimer ("topology::buildfacesc");
	NgProfiler::RegionTimer reg2 (timer2);

	if (id == 0)
	  PrintMessage (5, "Update faces ");

        NgProfiler::StartTimer (timer2a);

	faces.SetSize(ne);
	surffaces.SetSize(nse);
	
	int oldnfa = face2vert.Size();

	cnt = 0;
	for (int i = 0; i < face2vert.Size(); i++)
	  cnt[face2vert[i][0]]++;
	TABLE<int,PointIndex::BASE> vert2oldface(cnt);
	for (int i = 0; i < face2vert.Size(); i++)
	  vert2oldface.AddSave (face2vert[i][0], i);


	for (int elnr = 0; elnr < ne; elnr++)
	  for (int j = 0; j < 6; j++)
	    faces[elnr][j].fnr = -1;
	

	int max_face_on_vertex = 0;
	for (int i = PointIndex::BASE; i < nv+PointIndex::BASE; i++)
	  {
	    int onv = vert2oldface[i].Size() + (*vert2element)[i].Size() + (*vert2surfelement)[i].Size();
	    max_face_on_vertex = max (onv, max_face_on_vertex);
	  }
	




        NgProfiler::StopTimer (timer2a);
        NgProfiler::StartTimer (timer2b);

        /*
	for (int pass = 1; pass <= 2; pass++)
	  {
            cout << "pass = " << pass << endl;
	    nfa = oldnfa;
	    for (int v = PointIndex::BASE; v < nv+PointIndex::BASE; v++)
	      {
		int first_fa = nfa;

		INDEX_3_CLOSED_HASHTABLE<int> vert2face(2*max_face_on_vertex+10); 
		
		for (int j = 0; j < vert2oldface[v].Size(); j++)
		  {
		    int fnr = vert2oldface[v][j];
		    INDEX_3 face (face2vert[fnr].I1(),
				  face2vert[fnr].I2(),
				  face2vert[fnr].I3());
		    vert2face.Set (face, fnr+1);
		  }


		if (pass == 2)
                  {
                    // *testout << "pass 2, nfa = " << nfa << "; face2vert.Size() = " << face2vert.Size() << endl;
                    for (int j = nfa; j < face2vert.Size(); j++)
                      {
                        if (face2vert[j][0] == v)
                          {
                            INDEX_3 face (face2vert[j].I1(),
                                          face2vert[j].I2(),
                                          face2vert[j].I3());
                            vert2face.Set (face, j+1);
                            nfa++;
                          }
                        else
                          break;
                      }
                  }

		// cout << "inherited faces: " << endl << vert2face << endl;


		for (int j = 0; j < (*vert2element)[v].Size(); j++)
		  {
                    // NgProfiler::RegionTimer reg3 (timer2d);

		    ElementIndex elnr = (*vert2element)[v][j];
		    const Element & el = mesh[elnr];
	  
		    int nelfaces = GetNFaces (el.GetType());
		    const ELEMENT_FACE * elfaces = GetFaces0 (el.GetType());
	  
		    for (int j = 0; j < nelfaces; j++)
		      if (elfaces[j][3] < 0)
                        
			{ // triangle
			  int facenum, facedir;
			  INDEX_3 face(el[elfaces[j][0]], el[elfaces[j][1]], 
                                       el[elfaces[j][2]]);
                          
			  facedir = 0;
			  if (face.I1() > face.I2())
			    { swap (face.I1(), face.I2()); facedir += 1; }
			  if (face.I2() > face.I3())
			    { swap (face.I2(), face.I3()); facedir += 2; }
			  if (face.I1() > face.I2())
			    { swap (face.I1(), face.I2()); facedir += 4; }
		      
			  if (face.I1() != v) continue;
                          
			  if (vert2face.Used (face))
			    facenum = vert2face.Get(face);
			  else
			    {
                              if (pass == 2) cout << "hier in pass 2" << endl;
			      nfa++;
			      vert2face.Set (face, nfa);
			      facenum = nfa;
			  
			      INDEX_4 hface(face.I1(),face.I2(),face.I3(),0);
			      face2vert.Append (hface);
			    }
			  faces[elnr][j] = 8*(facenum-1)+facedir+1;
			}
                    
		      else
		    
			{
			  // quad
			  int facenum, facedir;
			  INDEX_4Q face4(el[elfaces[j][0]], el[elfaces[j][1]],
					 el[elfaces[j][2]], el[elfaces[j][3]]);
		      
			  facedir = 0;
			  if (min2 (face4.I1(), face4.I2()) > 
			      min2 (face4.I4(), face4.I3())) 
			    {  // z - flip
			      facedir += 1; 
			      swap (face4.I1(), face4.I4());
			      swap (face4.I2(), face4.I3());
			    }
			  if (min2 (face4.I1(), face4.I4()) >
			      min2 (face4.I2(), face4.I3())) 
			    {  // x - flip
			      facedir += 2; 
			      swap (face4.I1(), face4.I2());
			      swap (face4.I3(), face4.I4());
			    }
			  if (face4.I2() > face4.I4())
			    {  // diagonal flip
			      facedir += 4; 
			      swap (face4.I2(), face4.I4());
			    }

		
			  INDEX_3 face(face4.I1(), face4.I2(), face4.I3());

			  if (face.I1() != v) continue;
		      
			  if (vert2face.Used (face))
			    {
			      facenum = vert2face.Get(face);
			    }
			  else
			    {
                              if (pass == 2) cout << "hier in pass 2" << endl;
			      nfa++;
			      vert2face.Set (face, nfa);
			      facenum = nfa;
			  
			      INDEX_4 hface(face4.I1(),face4.I2(),face4.I3(),face4.I4());
			      face2vert.Append (hface);
			    }
		      
			  faces[elnr][j] = 8*(facenum-1)+facedir+1;
			}
		  }

		for (int j = 0; j < (*vert2surfelement)[v].Size(); j++)
		  {
		    int elnr = (*vert2surfelement)[v][j];
		    // cout << "surfelnr = " << elnr << endl;
		    const Element2d & el = mesh.SurfaceElement (elnr);
		
		    const ELEMENT_FACE * elfaces = GetFaces1 (el.GetType());
		
		    if (elfaces[0][3] == 0)
	    
		      { // triangle
	      
			int facenum;
			int facedir;
		    
			INDEX_3 face(el.PNum(elfaces[0][0]),
				     el.PNum(elfaces[0][1]),
				     el.PNum(elfaces[0][2]));
		    
			// cout << "face = " << face << endl;

			facedir = 0;
			if (face.I1() > face.I2())
			  {
			    swap (face.I1(), face.I2());
			    facedir += 1;
			  }
			if (face.I2() > face.I3())
			  {
			    swap (face.I2(), face.I3());
			    facedir += 2;
			  }
			if (face.I1() > face.I2())
			  {
			    swap (face.I1(), face.I2());
			    facedir += 4;
			  }
		    
			if (face.I1() != v) continue;
		    
			if (vert2face.Used (face))
			  facenum = vert2face.Get(face);
			else
			  {
			    nfa++;
			    vert2face.Set (face, nfa);
			    facenum = nfa;
			
			    INDEX_4 hface(face.I1(),face.I2(),face.I3(),0);
			    face2vert.Append (hface);
			  }

			surffaces.Elem(elnr) = 8*(facenum-1)+facedir+1;
		      }
	  
		    else
	    
		      {
			// quad
			int facenum;
			int facedir;
		    
			INDEX_4Q face4(el.PNum(elfaces[0][0]),
				       el.PNum(elfaces[0][1]),
				       el.PNum(elfaces[0][2]),
				       el.PNum(elfaces[0][3]));
		    
			facedir = 0;
			if (min2 (face4.I1(), face4.I2()) > 
			    min2 (face4.I4(), face4.I3())) 
			  {  // z - orientation
			    facedir += 1; 
			    swap (face4.I1(), face4.I4());
			    swap (face4.I2(), face4.I3());
			  }
			if (min2 (face4.I1(), face4.I4()) >
			    min2 (face4.I2(), face4.I3())) 
			  {  // x - orientation
			    facedir += 2; 
			    swap (face4.I1(), face4.I2());
			    swap (face4.I3(), face4.I4());
			  }
			if (face4.I2() > face4.I4())
			  { 
			    facedir += 4; 
			    swap (face4.I2(), face4.I4());
			  }
		    
			INDEX_3 face(face4.I1(), face4.I2(), face4.I3());
			if (face.I1() != v) continue;
		    
			if (vert2face.Used (face))
			  facenum = vert2face.Get(face);
			else
			  {
			    nfa++;
			    vert2face.Set (face, nfa);
			    facenum = nfa;
			
			    INDEX_4 hface(face4.I1(),face4.I2(),face4.I3(),face4.I3());
			    face2vert.Append (hface);
			  }
		    
			surffaces.Elem(elnr) = 8*(facenum-1)+facedir+1;
		      }
		  }

		// sort faces
		// *testout << "faces = " << face2vert << endl;
		if (pass == 1)
		  {
                    // *testout << "pass1, sort from " << first_fa << " to " << nfa << endl;
		    for (int i = first_fa; i < nfa; i++)
		      for (int j = first_fa+1; j < nfa; j++)
			if (face2vert[j] < face2vert[j-1])
			  Swap (face2vert[j-1], face2vert[j]);
		  }
		// *testout << "faces, sorted = " << face2vert << endl;
	      }


	    face2vert.SetAllocSize (nfa);            
	  }
        */

        /*
          // timing tests
	static int timer_touch = NgProfiler::CreateTimer ("topology::buildfaces - touch els");
	static int timer_touch2 = NgProfiler::CreateTimer ("topology::buildfaces - touch els vert");

        NgProfiler::StartTimer (timer_touch);

        int sum = 0;
        for (ElementIndex ei = 0; ei < mesh.GetNE(); ei++)
          {		    
            const Element & el = mesh[ei];
            for (int j = 0; j < el.GetNP(); j++)
              sum += el[j];
          }

        NgProfiler::StopTimer (timer_touch);

        NgProfiler::StartTimer (timer_touch2);

        for (int v = PointIndex::BASE; v < nv+PointIndex::BASE; v++)
          {
            INDEX_3_CLOSED_HASHTABLE<int> vert2face(2*max_face_on_vertex+10); 
            
            for (int pass = 1; pass <= 2; pass++)
              for (int j = 0; j < (*vert2element)[v].Size(); j++)
                {
                  // NgProfiler::RegionTimer reg3 (timer2d);
                  
                  ElementIndex elnr = (*vert2element)[v][j];
                  const Element & el = mesh[elnr];


                  int nelfaces = GetNFaces (el.GetType());
                  const ELEMENT_FACE * elfaces = GetFaces0 (TET);
	  
                  for (int j = 0; j < 4; j++)
                    { // triangle
                      INDEX_3 face(el[elfaces[j][0]], el[elfaces[j][1]], 
                                   el[elfaces[j][2]]);
                      
                      int facedir = 0;
                      if (face.I1() > face.I2())
                        { swap (face.I1(), face.I2()); facedir += 1; }
                      if (face.I2() > face.I3())
                        { swap (face.I2(), face.I3()); facedir += 2; }
                      if (face.I1() > face.I2())
                        { swap (face.I1(), face.I2()); facedir += 4; }
                      
                      sum += face.I1();
                      sum += face.I1() < face.I2();
                    }
                }
          }
        
        NgProfiler::StopTimer (timer_touch2);
        *testout << "sum" << sum << endl;
        */



        nfa = oldnfa;
        for (int v = PointIndex::BASE; v < nv+PointIndex::BASE; v++)
          {
            int first_fa = nfa;

            INDEX_3_CLOSED_HASHTABLE<int> vert2face(2*max_face_on_vertex+10); 
            
            for (int j = 0; j < vert2oldface[v].Size(); j++)
              {
                int fnr = vert2oldface[v][j];
                INDEX_3 face (face2vert[fnr].I1(),
                              face2vert[fnr].I2(),
                              face2vert[fnr].I3());
                vert2face.Set (face, fnr+1);
              }
            

            for (int pass = 1; pass <= 2; pass++)
	      {

		if (pass == 2)
                  {
                    for (int j = first_fa; j < face2vert.Size(); j++)
                      {
                        if (face2vert[j][0] == v)
                          {
                            INDEX_3 face (face2vert[j].I1(),
                                          face2vert[j].I2(),
                                          face2vert[j].I3());
                            vert2face.Set (face, j+1);
                          }
                        else
                          break;
                      }
                  }



		for (int j = 0; j < (*vert2element)[v].Size(); j++)
		  {
                    // NgProfiler::RegionTimer reg3 (timer2d);

		    ElementIndex elnr = (*vert2element)[v][j];
		    const Element & el = mesh[elnr];
	  
		    int nelfaces = GetNFaces (el.GetType());
		    const ELEMENT_FACE * elfaces = GetFaces0 (el.GetType());
	  
		    for (int j = 0; j < nelfaces; j++)
		      if (elfaces[j][3] < 0)
                        
			{ // triangle
			  INDEX_3 face(el[elfaces[j][0]], el[elfaces[j][1]], 
                                       el[elfaces[j][2]]);
                          
			  int facedir = 0;
			  if (face.I1() > face.I2())
			    { swap (face.I1(), face.I2()); facedir += 1; }
			  if (face.I2() > face.I3())
			    { swap (face.I2(), face.I3()); facedir += 2; }
			  if (face.I1() > face.I2())
			    { swap (face.I1(), face.I2()); facedir += 4; }
		      
			  if (face.I1() != v) continue;

                          if (pass == 1)
                            {
                              if (!vert2face.Used (face))
                                {
                                  nfa++;
                                  vert2face.Set (face, nfa);
                                  INDEX_4 hface(face.I1(),face.I2(),face.I3(),0);
                                  face2vert.Append (hface);
                                }
                            }
                          else
                            {
                              int facenum = vert2face.Get(face);
                              // faces[elnr][j] = 8*(facenum-1)+facedir+1;
                              faces[elnr][j].fnr = facenum-1;
                              faces[elnr][j].forient = facedir;
                            }
			}
                    
		      else
		    
			{
			  // quad
			  int facenum;
			  INDEX_4Q face4(el[elfaces[j][0]], el[elfaces[j][1]],
					 el[elfaces[j][2]], el[elfaces[j][3]]);
		      
			  int facedir = 0;
			  if (min2 (face4.I1(), face4.I2()) > 
			      min2 (face4.I4(), face4.I3())) 
			    {  // z - flip
			      facedir += 1; 
			      swap (face4.I1(), face4.I4());
			      swap (face4.I2(), face4.I3());
			    }
			  if (min2 (face4.I1(), face4.I4()) >
			      min2 (face4.I2(), face4.I3())) 
			    {  // x - flip
			      facedir += 2; 
			      swap (face4.I1(), face4.I2());
			      swap (face4.I3(), face4.I4());
			    }
			  if (face4.I2() > face4.I4())
			    {  // diagonal flip
			      facedir += 4; 
			      swap (face4.I2(), face4.I4());
			    }

		
			  INDEX_3 face(face4.I1(), face4.I2(), face4.I3());

			  if (face.I1() != v) continue;
		      
			  if (vert2face.Used (face))
			    {
			      facenum = vert2face.Get(face);
			    }
			  else
			    {
                              if (pass == 2) cout << "hier in pass 2" << endl;
			      nfa++;
			      vert2face.Set (face, nfa);
			      facenum = nfa;
			  
			      INDEX_4 hface(face4.I1(),face4.I2(),face4.I3(),face4.I4());
			      face2vert.Append (hface);
			    }
		      
			  // faces[elnr][j] = 8*(facenum-1)+facedir+1;
                          faces[elnr][j].fnr = facenum-1;
                          faces[elnr][j].forient = facedir;
			}
		  }

		for (int j = 0; j < (*vert2surfelement)[v].Size(); j++)
		  {
		    int elnr = (*vert2surfelement)[v][j];
		    // cout << "surfelnr = " << elnr << endl;
		    const Element2d & el = mesh.SurfaceElement (elnr);
		
		    const ELEMENT_FACE * elfaces = GetFaces1 (el.GetType());
		
		    if (elfaces[0][3] == 0)
	    
		      { // triangle
	      
			int facenum;
			int facedir;
		    
			INDEX_3 face(el.PNum(elfaces[0][0]),
				     el.PNum(elfaces[0][1]),
				     el.PNum(elfaces[0][2]));
		    
			// cout << "face = " << face << endl;

			facedir = 0;
			if (face.I1() > face.I2())
			  {
			    swap (face.I1(), face.I2());
			    facedir += 1;
			  }
			if (face.I2() > face.I3())
			  {
			    swap (face.I2(), face.I3());
			    facedir += 2;
			  }
			if (face.I1() > face.I2())
			  {
			    swap (face.I1(), face.I2());
			    facedir += 4;
			  }
		    
			if (face.I1() != v) continue;
		    
			if (vert2face.Used (face))
			  facenum = vert2face.Get(face);
			else
			  {
			    nfa++;
			    vert2face.Set (face, nfa);
			    facenum = nfa;
			
			    INDEX_4 hface(face.I1(),face.I2(),face.I3(),0);
			    face2vert.Append (hface);
			  }

			// surffaces.Elem(elnr) = 8*(facenum-1)+facedir+1;
                        surffaces.Elem(elnr).fnr = facenum-1;
                        surffaces.Elem(elnr).forient = facedir;
		      }
	  
		    else
	    
		      {
			// quad
			int facenum;
			int facedir;
		    
			INDEX_4Q face4(el.PNum(elfaces[0][0]),
				       el.PNum(elfaces[0][1]),
				       el.PNum(elfaces[0][2]),
				       el.PNum(elfaces[0][3]));
		    
			facedir = 0;
			if (min2 (face4.I1(), face4.I2()) > 
			    min2 (face4.I4(), face4.I3())) 
			  {  // z - orientation
			    facedir += 1; 
			    swap (face4.I1(), face4.I4());
			    swap (face4.I2(), face4.I3());
			  }
			if (min2 (face4.I1(), face4.I4()) >
			    min2 (face4.I2(), face4.I3())) 
			  {  // x - orientation
			    facedir += 2; 
			    swap (face4.I1(), face4.I2());
			    swap (face4.I3(), face4.I4());
			  }
			if (face4.I2() > face4.I4())
			  { 
			    facedir += 4; 
			    swap (face4.I2(), face4.I4());
			  }
		    
			INDEX_3 face(face4.I1(), face4.I2(), face4.I3());
			if (face.I1() != v) continue;
		    
			if (vert2face.Used (face))
			  facenum = vert2face.Get(face);
			else
			  {
			    nfa++;
			    vert2face.Set (face, nfa);
			    facenum = nfa;
			
			    INDEX_4 hface(face4.I1(),face4.I2(),face4.I3(),face4.I3());
			    face2vert.Append (hface);
			  }
		    
			// surffaces.Elem(elnr) = 8*(facenum-1)+facedir+1;
                        surffaces.Elem(elnr).fnr = facenum-1;
                        surffaces.Elem(elnr).forient = facedir;
		      }
		  }

		// sort faces
		if (pass == 1)
		  {
                    for (int i = 0; i < nfa-first_fa; i++)
		      for (int j = first_fa+1; j < nfa-i; j++)
			if (face2vert[j] < face2vert[j-1])
			  Swap (face2vert[j-1], face2vert[j]);
		  }
	      }
	  }


        face2vert.SetAllocSize (nfa);            





	// *testout << "face2vert = " << endl << face2vert << endl;



        NgProfiler::StopTimer (timer2b);
        NgProfiler::StartTimer (timer2c);




	
	face2surfel.SetSize (nfa);
	face2surfel = 0;
	for (int i = 1; i <= nse; i++)
	  face2surfel.Elem(GetSurfaceElementFace(i)) = i;

	/*
	  cout << "build table complete" << endl;

	  cout << "faces = " << endl;

	  cout << "face2vert = " << endl << face2vert << endl;
	  cout << "surffaces = " << endl << surffaces << endl;
	  cout << "face2surfel = " << endl << face2surfel << endl;
	*/

	
	surf2volelement.SetSize (nse);
	for (int i = 1; i <= nse; i++)
	  {
	    surf2volelement.Elem(i)[0] = 0;
	    surf2volelement.Elem(i)[1] = 0;
	  }
	for (int i = 1; i <= ne; i++)
	  for (int j = 0; j < 6; j++)
	    {
	      // int fnum = (faces.Get(i)[j]+7) / 8;
              int fnum = faces.Get(i)[j].fnr+1;
	      if (fnum > 0 && face2surfel.Elem(fnum))
		{
		  int sel = face2surfel.Elem(fnum);
		  surf2volelement.Elem(sel)[1] = 
		    surf2volelement.Elem(sel)[0];
		  surf2volelement.Elem(sel)[0] = i;
		}
	    }

	face2vert.SetAllocSize (face2vert.Size());

	// face table complete


#ifdef PARALLEL
	// (*testout) << " RESET Paralleltop" << endl;
	// paralleltop.Reset ();
#endif

	Array<short int> face_els(nfa), face_surfels(nfa);
	face_els = 0;
	face_surfels = 0;
	Array<int> hfaces;
	for (int i = 1; i <= ne; i++)
	  {
	    GetElementFaces (i, hfaces);
	    for (int j = 0; j < hfaces.Size(); j++)
	      face_els[hfaces[j]-1]++;
	  }
	for (int i = 1; i <= nse; i++)
	  face_surfels[GetSurfaceElementFace (i)-1]++;


	if (ne)
	  {
	    int cnt_err = 0;
	    for (int i = 0; i < nfa; i++)
	      {
		/*
		  (*testout) << "face " << i << " has " << int(face_els[i]) << " els, " 
		  << int(face_surfels[i]) << " surfels, tot = "
		  << face_els[i] + face_surfels[i] << endl; 
		*/
		if (face_els[i] + face_surfels[i] == 1)
		  {
		    cnt_err++;
#ifdef PARALLEL
		    if ( ntasks > 1 )
		      {
			continue;
			// if ( !paralleltop.DoCoarseUpdate() ) continue;
		      }
		    else
#endif
		      {
			(*testout) << "illegal face : " << i << endl;
			(*testout) << "points = " << face2vert[i] << endl;
			(*testout) << "pos = ";
			for (int j = 0; j < 4; j++)
			  if (face2vert[i].I(j+1) >= 1)
			    (*testout) << mesh[(PointIndex)face2vert[i].I(j+1)] << " ";
			(*testout) << endl;

			FlatArray<ElementIndex> vertels = GetVertexElements (face2vert[i].I(1));
			for (int k = 0; k < vertels.Size(); k++)
			  {
			    int elfaces[10], orient[10];
			    int nf = GetElementFaces (vertels[k]+1, elfaces, orient);
			    for (int l = 0; l < nf; l++)
			      if (elfaces[l] == i)
				{
				  // (*testout) << "is face of element " << vertels[k] << endl;
			    
				  if (mesh.coarsemesh && mesh.hpelements->Size() == mesh.GetNE() )
				    {
				      const HPRefElement & hpref_el =
					(*mesh.hpelements) [ mesh[vertels[k]].hp_elnr];
				      (*testout) << "coarse eleme = " << hpref_el.coarse_elnr << endl;
				    }

				}
			  }
		      }
		  }
	      }

	    if (cnt_err && ntasks == 1)
	      cout << cnt_err << " elements are not matching !!!" << endl;
	  }
        NgProfiler::StopTimer (timer2c);
      }
    

#ifdef PARALLEL
    if (id != 0)  
      {
	// if ( paralleltop.DoCoarseUpdate() )
	// paralleltop.UpdateCoarseGrid();
      }
#endif
 
 
  
    /* 
       for (i = 1; i <= ne; i++)
       {
       (*testout) << "Element " << i << endl;
       (*testout) << "PNums " << endl; 
       for( int l=1;l<=8;l++) *testout << mesh.VolumeElement(i).PNum(l) << "\t"; 
       *testout << endl; 
       (*testout) << "edges: " << endl;
       for (j = 0; j < 9; j++)
       (*testout) << edges.Elem(i)[j] << " ";
       (*testout) << "faces: " << endl;
       for (j = 0; j < 6; j++)m
       (*testout) << faces.Elem(i)[j] << " ";
       }

       for (i = 1; i <= nse; i++)
       {
       (*testout) << "SElement " << i << endl;
       (*testout) << "PNums " << endl; 
       for( int l=1;l<=4;l++) *testout << mesh.SurfaceElement(i).PNum(l) << "\t"; 
       *testout << endl; 
       }
    */
    timestamp = NextTimeStamp();
  }

  



  const Point3d * MeshTopology :: GetVertices (ELEMENT_TYPE et)
  {
    static Point3d segm_points [] = 
      { Point3d (1, 0, 0),
	Point3d (0, 0, 0) };
  
    static Point3d trig_points [] = 
      { Point3d ( 1, 0, 0 ),
	Point3d ( 0, 1, 0 ),
	Point3d ( 0, 0, 0 ) };

    static Point3d quad_points [] = 
      { Point3d ( 0, 0, 0 ),
	Point3d ( 1, 0, 0 ),
	Point3d ( 1, 1, 0 ),
	Point3d ( 0, 1, 0 ) };

    static Point3d tet_points [] = 
      { Point3d ( 1, 0, 0 ),
	Point3d ( 0, 1, 0 ),
	Point3d ( 0, 0, 1 ),
	Point3d ( 0, 0, 0 ) };

    static Point3d pyramid_points [] =
      {
	Point3d ( 0, 0, 0 ),
	Point3d ( 1, 0, 0 ),
	Point3d ( 1, 1, 0 ),
	Point3d ( 0, 1, 0 ),
	Point3d ( 0, 0, 1-1e-7 ),
      };    
  
    static Point3d prism_points[] = 
      {
	Point3d ( 1, 0, 0 ),
	Point3d ( 0, 1, 0 ),
	Point3d ( 0, 0, 0 ),
	Point3d ( 1, 0, 1 ),
	Point3d ( 0, 1, 1 ),
	Point3d ( 0, 0, 1 )
      };


    static Point3d hex_points [] = 
      { Point3d ( 0, 0, 0 ),
	Point3d ( 1, 0, 0 ),
	Point3d ( 1, 1, 0 ),
	Point3d ( 0, 1, 0 ),
	Point3d ( 0, 0, 1 ),
	Point3d ( 1, 0, 1 ),
	Point3d ( 1, 1, 1 ),
	Point3d ( 0, 1, 1 ) };


    switch (et)
      {
      case SEGMENT:
      case SEGMENT3:
	return segm_points;

      case TRIG:
      case TRIG6:
	return trig_points;

      case QUAD:
      case QUAD6:
      case QUAD8:
	return quad_points;

      case TET:
      case TET10:
	return tet_points;

      case PYRAMID:
	return pyramid_points;

      case PRISM:
      case PRISM12:
	return prism_points;

      case HEX:
	return hex_points;
      default:
	cerr << "Ng_ME_GetVertices, illegal element type " << et << endl;
      }
    return 0;
  }








  void MeshTopology :: GetElementEdges (int elnr, Array<int> & eledges) const
  {
    int ned = GetNEdges (mesh.VolumeElement(elnr).GetType());
    eledges.SetSize (ned);
    for (int i = 0; i < ned; i++)
      eledges[i] = edges.Get(elnr)[i].nr+1;
      // eledges[i] = abs (edges.Get(elnr)[i]);
  }
  void MeshTopology :: GetElementFaces (int elnr, Array<int> & elfaces, bool withorientation) const
  {
    int nfa = GetNFaces (mesh.VolumeElement(elnr).GetType());
    elfaces.SetSize (nfa);

    if (!withorientation)

      for (int i = 1; i <= nfa; i++)
	{
	  // elfaces.Elem(i) = (faces.Get(elnr)[i-1]-1) / 8 + 1;
          elfaces.Elem(i) = faces.Get(elnr)[i-1].fnr+1;
	}
    
    else
      {
        cerr << "GetElementFaces with orientation currently not supported" << endl;
        /*
          for (int i = 1; i <= nfa; i++)
          {
	  elfaces.Elem(i) = (faces.Get(elnr)[i-1]-1) / 8 + 1;
	  int orient = (faces.Get(elnr)[i-1]-1) % 8;
	  if(orient == 1 || orient == 2 || orient == 4 || orient == 7)
          elfaces.Elem(i) *= -1;
          }
        */
      }
  }

  void MeshTopology :: GetElementEdgeOrientations (int elnr, Array<int> & eorient) const
  {
    int ned = GetNEdges (mesh.VolumeElement(elnr).GetType());
    eorient.SetSize (ned);
    for (int i = 1; i <= ned; i++)
      // eorient.Elem(i) = (edges.Get(elnr)[i-1] > 0) ? 1 : -1;
      eorient.Elem(i) = (edges.Get(elnr)[i-1].orient) ? -1 : 1;
  }

  void MeshTopology :: GetElementFaceOrientations (int elnr, Array<int> & forient) const
  {
    int nfa = GetNFaces (mesh.VolumeElement(elnr).GetType());
    forient.SetSize (nfa);
    for (int i = 1; i <= nfa; i++)
      forient.Elem(i) = faces.Get(elnr)[i-1].forient;
    // forient.Elem(i) = (faces.Get(elnr)[i-1]-1) % 8;
  }



  int MeshTopology :: GetElementEdges (int elnr, int * eledges, int * orient) const
  {
    //  int ned = GetNEdges (mesh.VolumeElement(elnr).GetType());
    
    if (mesh.GetDimension()==3 || 1)
      {
        if (orient)
	  {
	    for (int i = 0; i < 12; i++)
	      {
                /*
		if (!edges.Get(elnr)[i]) return i;
		eledges[i] = abs (edges.Get(elnr)[i]);
		orient[i] = (edges.Get(elnr)[i] > 0 ) ? 1 : -1;
                */
                if (edges.Get(elnr)[i].nr == -1) return i;
                eledges[i] = edges.Get(elnr)[i].nr+1;
		orient[i] = edges.Get(elnr)[i].orient ? -1 : 1;
	      }
	  }
	else
	  {
	    for (int i = 0; i < 12; i++)
	      {
		// if (!edges.Get(elnr)[i]) return i;
		// eledges[i] = abs (edges.Get(elnr)[i]);
                if (edges.Get(elnr)[i].nr == -1) return i;
                eledges[i] = edges.Get(elnr)[i].nr+1;

	      }
	  }
	return 12;
      }
    else
      {
	throw NgException("rethink implementation");
	/*
	  if (orient)
	  {
	  for (i = 0; i < 4; i++)
	  {
	  if (!surfedges.Get(elnr)[i]) return i;
	  eledges[i] = abs (surfedges.Get(elnr)[i]);
	  orient[i] = (surfedges.Get(elnr)[i] > 0 ) ? 1 : -1;
	  }
	  }
	  else
	  {
	  if (!surfedges.Get(elnr)[i]) return i;
	  for (i = 0; i < 4; i++)
	  eledges[i] = abs (surfedges.Get(elnr)[i]);
	  }
	*/
	return 4;
	//      return GetSurfaceElementEdges (elnr, eledges, orient);
      }
  }

  int MeshTopology :: GetElementFaces (int elnr, int * elfaces, int * orient) const
  {
    //  int nfa = GetNFaces (mesh.VolumeElement(elnr).GetType());
    if (orient)
      {
	for (int i = 0; i < 6; i++)
	  {
            /*
	    if (!faces.Get(elnr)[i]) return i;
	    elfaces[i] = (faces.Get(elnr)[i]-1) / 8 + 1;
	    orient[i] = (faces.Get(elnr)[i]-1) % 8;
            */
	    if (faces.Get(elnr)[i].fnr == -1) return i;
	    elfaces[i] = faces.Get(elnr)[i].fnr+1;
	    orient[i] = faces.Get(elnr)[i].forient;
	  }
      }
    else
      {
	for (int i = 0; i < 6; i++)
	  {
	    // if (!faces.Get(elnr)[i]) return i;
	    // elfaces[i] = (faces.Get(elnr)[i]-1) / 8 + 1;
	    if (faces.Get(elnr)[i].fnr == -1) return i;
	    elfaces[i] = faces.Get(elnr)[i].fnr+1;
	  }
      }
    return 6;
  }

  void MeshTopology :: GetSurfaceElementEdges (int elnr, Array<int> & eledges) const
  {
    int ned = GetNEdges (mesh.SurfaceElement(elnr).GetType());
    eledges.SetSize (ned);
    for (int i = 0; i < ned; i++)
      // eledges[i] = abs (surfedges.Get(elnr)[i]);
      eledges[i] = surfedges.Get(elnr)[i].nr+1;
  }

  void MeshTopology :: GetEdges (SurfaceElementIndex elnr, Array<int> & eledges) const
  {
    int ned = GetNEdges (mesh[elnr].GetType());
    eledges.SetSize (ned);
    for (int i = 0; i < ned; i++)
      // eledges[i] = abs (surfedges[elnr][i])-1;
      eledges[i] = surfedges[elnr][i].nr;
  }

  int MeshTopology :: GetSurfaceElementFace (int elnr) const
  {
    return surffaces.Get(elnr).fnr+1;
  }

  int MeshTopology :: GetFace (SurfaceElementIndex elnr) const
  {
    return surffaces[elnr].fnr;
  }


  void MeshTopology :: 
  GetSurfaceElementEdgeOrientations (int elnr, Array<int> & eorient) const
  {
    int ned = GetNEdges (mesh.SurfaceElement(elnr).GetType());
    eorient.SetSize (ned);
    for (int i = 0; i < ned; i++)
      // eorient[i] = (surfedges.Get(elnr)[i] > 0) ? 1 : -1;
      eorient[i] = (surfedges.Get(elnr)[i].orient) ? -1 : 1;
  }

  int MeshTopology :: GetSurfaceElementFaceOrientation (int elnr) const
  {
    // return (surffaces.Get(elnr)-1) % 8;
    return surffaces.Get(elnr).forient;
  }

  int MeshTopology :: GetSurfaceElementEdges (int elnr, int * eledges, int * orient) const
  {
    int i;
    if (mesh.GetDimension() == 3 || 1)
      {
	if (orient)
	  {
	    for (i = 0; i < 4; i++)
	      {
                /*
		if (!surfedges.Get(elnr)[i]) return i;
		eledges[i] = abs (surfedges.Get(elnr)[i]);
		orient[i] = (surfedges.Get(elnr)[i] > 0 ) ? 1 : -1;
                */
		if (surfedges.Get(elnr)[i].nr == -1) return i;
		eledges[i] = surfedges.Get(elnr)[i].nr+1;
		orient[i] = (surfedges.Get(elnr)[i].orient) ? -1 : 1;

	      }
	  }
	else
	  {
	    for (i = 0; i < 4; i++)
	      {
                /*
		if (!surfedges.Get(elnr)[i]) return i;
		eledges[i] = abs (surfedges.Get(elnr)[i]);
                */
		if (surfedges.Get(elnr)[i].nr == -1) return i;
		eledges[i] = surfedges.Get(elnr)[i].nr+1;
	      }
	  }
	return 4;
      }
    else
      {
        /*
	eledges[0] = abs (segedges.Get(elnr));
	if (orient)
	  orient[0] = segedges.Get(elnr) > 0 ? 1 : -1;
        */
	eledges[0] = segedges.Get(elnr).nr+1;
	if (orient)
	  orient[0] = segedges.Get(elnr).orient ? -1 : 1;
      }
    return 1;
  }


  void MeshTopology :: GetFaceVertices (int fnr, Array<int> & vertices) const
  {
    vertices.SetSize(4);
    for (int i = 0; i < 4; i++)
      vertices[i] = face2vert.Get(fnr)[i];
    if (vertices[3] == 0)
      vertices.SetSize(3);
  }

  void MeshTopology :: GetFaceVertices (int fnr, int * vertices) const
  {
    for (int i = 0; i <= 3; i++)
      vertices[i] = face2vert.Get(fnr)[i];
  }


  void MeshTopology :: GetEdgeVertices (int ednr, int & v1, int & v2) const
  {
    // cout << "id = " << id << "getedgevertices, ednr = " << ednr << ", ned = " << edge2vert.Size() << "&v1 = " << &v1 << endl;
    if (ednr < 1 || ednr > edge2vert.Size())
      cerr << "illegal edge nr: " << ednr << ", numedges = " << edge2vert.Size() 
	   << " id = " << id 
	   << endl;
    v1 = edge2vert.Get(ednr)[0];
    v2 = edge2vert.Get(ednr)[1];
  }

  void MeshTopology :: GetEdgeVertices (int ednr, PointIndex & v1, PointIndex & v2) const
  {
    v1 = edge2vert.Get(ednr)[0];
    v2 = edge2vert.Get(ednr)[1];
  }


  void MeshTopology :: GetFaceEdges (int fnr, Array<int> & fedges, bool withorientation) const
  {
    ArrayMem<int,4> pi(4);
    ArrayMem<int,12> eledges;
  
    fedges.SetSize (0);
    GetFaceVertices(fnr, pi);

    // Sort Edges according to global vertex numbers 
    // e1 = fmax, f2 
    // e2 = fmax, f1 
    // e3 = op e1(f2,f3) 
    // e4 = op e2(f1,f3) 

    /*  ArrayMem<int,4> fp; 
	fp[0] = pi[0]; 
	for(int k=1;k<pi.Size();k++) 
	if(fp[k]>fp[0]) swap(fp[k],fp[0]); 
  
	fp[1] = fp[0]+ */ 
  

    //  GetVertexElements (pi[0], els);
    FlatArray<ElementIndex> els = GetVertexElements (pi[0]);

    // find one element having all vertices of the face
    for (int i = 0; i < els.Size(); i++)
      {
	const Element & el = mesh[els[i]];
	int nref_faces = GetNFaces (el.GetType());
	const ELEMENT_FACE * ref_faces = GetFaces1 (el.GetType());
	int nfa_ref_edges = GetNEdges (GetFaceType(fnr));
      
	int cntv = 0,fa=-1; 
	for(int m=0;m<nref_faces;m++)
	  { 
	    cntv=0;
	    for(int j=0;j<nfa_ref_edges && ref_faces[m][j]>0;j++)
	      for(int k=0;k<pi.Size();k++)
		{
		  if(el[ref_faces[m][j]-1] == pi[k])
		    cntv++;
		}
	    if (cntv == pi.Size())
	      {
		fa=m;
		break;
	      }
	  }
     
	if(fa>=0)
	  {
	    const ELEMENT_EDGE * fa_ref_edges = GetEdges1 (GetFaceType(fnr)); 
	    fedges.SetSize(nfa_ref_edges);
	    GetElementEdges (els[i]+1, eledges);
	  
	    for (int j = 0; j < eledges.Size(); j++)
	      {
		int vi1, vi2;
		GetEdgeVertices (eledges[j], vi1, vi2);
	    
		bool has1 = 0;
		bool has2 = 0;
		for (int k = 0; k < pi.Size(); k++)
		  {
		    if (vi1 == pi[k]) has1 = 1;
		    if (vi2 == pi[k]) has2 = 1;
		  
		  }
	      
		if (has1 && has2) // eledges[j] is on face 
		  {
		    // fedges.Append (eledges[j]);
		    for(int k=0;k<nfa_ref_edges;k++)
		      {
			int w1 = el[ref_faces[fa][fa_ref_edges[k][0]-1]-1]; 
			int w2 = el[ref_faces[fa][fa_ref_edges[k][1]-1]-1]; 

			if(withorientation)
			  {
			    if(w1==vi1 && w2==vi2)
			      fedges[k] = eledges[j];
			    if(w1==vi2 && w2==vi1)
			      fedges[k] = -eledges[j];
			  }
			else
			  if((w1==vi1 && w2==vi2) || (w1==vi2 && w2==vi1))
			    fedges[k] = eledges[j];
		      }
		  }
	      }
	  
	    // *testout << " Face " << fnr << endl; 
	    // *testout << " GetFaceEdges " << fedges << endl;
	  
	    return;
	  }
      }   

    int surfel = GetFace2SurfaceElement(fnr);
    if (surfel != 0)
      {
	GetSurfaceElementEdges (surfel, fedges);
	return;
      }
  }


  ELEMENT_TYPE MeshTopology :: GetFaceType (int fnr) const
  {
    if (face2vert.Get(fnr)[3] == 0) return TRIG; else return QUAD;
  }


  void MeshTopology :: GetVertexElements (int vnr, Array<ElementIndex> & elements) const
  {
    if (vert2element)
      {
	int ne = vert2element->EntrySize(vnr);
	elements.SetSize(ne);
	for (int i = 1; i <= ne; i++)
	  elements.Elem(i) = vert2element->Get(vnr, i);
      }
  }


  FlatArray<ElementIndex> MeshTopology :: GetVertexElements (int vnr) const
  {
    if (vert2element)
      return (*vert2element)[vnr];
    return FlatArray<ElementIndex> (0,0);
  }

  FlatArray<int> MeshTopology :: GetVertexSurfaceElements (int vnr) const
  {
    if (vert2surfelement)
      return (*vert2surfelement)[vnr];
    return FlatArray<int> (0,0);
  }


  void MeshTopology :: GetVertexSurfaceElements( int vnr, 
						 Array<int>& elements ) const
  {
    if (vert2surfelement)
      {
	int i;
	int ne = vert2surfelement->EntrySize(vnr);
	elements.SetSize(ne);
	for (i = 1; i <= ne; i++)
	  elements.Elem(i) = vert2surfelement->Get(vnr, i);
      }
  }


  int MeshTopology :: GetVerticesEdge ( int v1, int v2 ) const
  {
    Array<ElementIndex> elements_v1;
    Array<int> elementedges;
    GetVertexElements ( v1, elements_v1);
    int edv1, edv2;

    for ( int i = 0; i < elements_v1.Size(); i++ )
      {
	GetElementEdges( elements_v1[i]+1, elementedges );
	for ( int ed = 0; ed < elementedges.Size(); ed ++)
	  {
	    GetEdgeVertices( elementedges[ed], edv1, edv2 );
	    if ( ( edv1 == v1 && edv2 == v2 ) || ( edv1 == v2 && edv2 == v1 ) )
	      return elementedges[ed];
	  }
      }

    return -1;
  }



  void MeshTopology :: 
  GetSegmentVolumeElements ( int segnr, Array<int> & volels ) const
  {
    int v1, v2;
    GetEdgeVertices ( GetSegmentEdge (segnr), v1, v2 );
    Array<ElementIndex> volels1, volels2;
    GetVertexElements ( v1, volels1 );
    GetVertexElements ( v2, volels2 );
    volels.SetSize(0);

    for ( int eli1=1; eli1 <= volels1.Size(); eli1++)
      if ( volels2.Contains( volels1.Elem(eli1) ) )
	volels.Append ( volels1.Elem(eli1)+1 );
  }

  void MeshTopology :: 
  GetSegmentSurfaceElements (int segnr, Array<int> & els) const
  {
    int v1, v2;
    GetEdgeVertices ( GetSegmentEdge (segnr), v1, v2 );
    Array<int> els1, els2;
    GetVertexSurfaceElements ( v1, els1 );
    GetVertexSurfaceElements ( v2, els2 );
    els.SetSize(0);

    for ( int eli1=1; eli1 <= els1.Size(); eli1++)
      if ( els2.Contains( els1.Elem(eli1) ) )
	els.Append ( els1.Elem(eli1) );
  }




}
