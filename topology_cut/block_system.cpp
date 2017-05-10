#include "geometry.h"
#include "block_system.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <list>
#include <CGAL\Timer.h>
using namespace std;

namespace TC
{


int block_system::init_domain (std::ifstream & infile) 
{
	domain.clear();
	int res1 = domain.init_stl (infile);
	int res2 = domain.check ();
	if (res1 == 0 && res2 == 0)
	{
		for (int i(0);i!=domain.vt.size ();++i)		//��¼ÿ�����ת��
		{
			plane_3 temp (domain.get_plane(i));
			domain.vpli.push_back (add_set_pl(vpl,temp));
			if (temp.orthogonal_vector() * vpl[domain.vpli.back ()].orthogonal_vector() > 0)
				domain.vtri_ori.push_back (true);
			else
				domain.vtri_ori .push_back (false);			
		}
		return 0;
	}
	else
		return 1;
}

int block_system::add_polyf (const poly_frac& pf, const poly_frac::Type &type)
{
	vector_3 normal (oriented_normal (pf.vl[0]));
	if (normal == vector_3 (0,0,0))
		return 1;
	
	vpf.push_back (pf);
	vpf.back ().plane_id = add_set_pl(vpl,plane_3(pf.vl[0][0],normal));
	vpf.back ().frac_id = (int) vpf.size ();
	vpf.back ().type = type;
	vpf.back ().proj_plane(vpl[vpf.back ().plane_id]);		//�Է���һ

	if (normal * vpl[vpf.back ().plane_id].orthogonal_vector() < 0)
		vpf.back ().revise_loops();		//��Ҫ��ת����loop

	return 0;
}

int block_system::identify ()
{
	ofstream outfile;						//DEBUG

	cout<<"Init. cross section"<<endl;
	vsec.resize (vpl.size ());
	for (int i(0);i!=vpl.size ();++i)
		init_csections(i,vsec[i]);
	cout<<"Init. cross section complete"<<endl;

	cout<<"Init. cross sections for fractures"<<endl;
	init_vsec_pf();
	cout<<"Init. cross sections for fractures complete"<<endl;

	cout<<"Generating edges"<<endl;
	gen_edges();
	cout<<"Edge generation complete"<<endl;

	CGAL::Timer timer;
	//���ɸ���face
	timer.reset ();
	timer.start();
	for (int i(0);i!=vpl.size ();++i)
	{
		cout<<i<<endl;
		vector<loop> vl;
		gen_loop (i,vl);
		loop2face (i,vl);
	}
	timer.stop();
	cout<<"tracing face:"<<timer.time()<<endl;
	timer.reset ();

	//DEBUG
	outfile.open("face_debug.stl");			//DEBUG
	for (int i(0);i!=vfa.size ();++i)		//DEBUG
		output_fac(outfile,i);				//DEBUG
	outfile.close ();						//DEBUG
	outfile.open ("edge_debug.txt");		//DEBUG
	output_ved(outfile);					//DEBUG
	outfile.close ();						//DEBUG

	timer.start ();
	gen_blk();
	cout<<"tracing blks:"<<timer.time ()<<endl;
	timer.stop (); timer.reset ();
	timer.start ();
	extract_outer_surfaces();
	cout<<"extracting blks' outer surfaces:"<<timer.time ()<<endl;
	return 0;
}

int block_system::disc_frac_parser (std::ifstream &infile, const FT& dx, const int&n)
{
	if (!infile)
		return 1;
	int count(0);
	infile>>count;
	for (int i(0);i!=count;++i)
	{
		cout<<i<<"/"<<count<<endl;
		if (!infile)
			return 0;
		poly_frac temp;
		FT x,y,z,dip_dir,dip,r;
		infile>>x>>y>>z>>dip_dir>>dip>>r>>temp.aperture >>temp.cohesion>>temp.f_angle;
		disc2pf(point_3 (x,y,z),r,plane_3 (point_3(x,y,z),dip2normal(dip_dir,dip,dx)),temp,n);
		add_polyf(temp);
	}
	return 0;
}

int block_system::output_ved (std::ofstream &outfile) const
{
	if (!outfile)
		return 1;
	for (int i(0);i!=ved.size ();++i)
		out_line_3_rhino(outfile,segment_3(vpo[ved[i].n[0]],vpo[ved[i].n[1]]));
	return 0;
}

int block_system::output_fac (std::ofstream &outfile, const int&i) const
{
	stringstream ss;
	ss<<i;
	out_face_STL(outfile,vpo,vfa[i],vpl[vfa[i].pli],true,ss.str());
	return 0;
}

int block_system::output_blk (std::ofstream &outfile, const int &bi) const
{
	stringstream ss;
	ss<<bi;
	outfile<<"solid BLK"<<ss.str()<<endl;
	for (int i(0);i!=vblk[bi].vfi.size ();++i)
	{
		plane_3 pl;
		if (vblk[bi].vfi[i].second )
			pl = vpl[vfa[vblk[bi].vfi[i].first ].pli];
		else
			pl = vpl[vfa[vblk[bi].vfi[i].first ].pli].opposite();

		out_face_STL(outfile,vpo,vfa[vblk[bi].vfi[i].first],pl,false);
	}
	outfile<<"endsolid BLK"<<ss.str ()<<endl;
	return 0;

}

int block_system::output_outer_bounds (std::ofstream &outfile, const int &bi) const
{
	stringstream ss;
	ss<<bi;
	outfile<<"solid BLK"<<ss.str()<<endl;
	for (int i(0);i!=vblk_ext[bi].vfi.size ();++i)
	{
		plane_3 pl;
		if (vblk_ext[bi].vfi[i].second )
			pl = vpl[vfa_ext[vblk_ext[bi].vfi[i].first ].pli];
		else
			pl = vpl[vfa_ext[vblk_ext[bi].vfi[i].first ].pli].opposite();

		out_face_STL(outfile,vpo,vfa_ext[vblk_ext[bi].vfi[i].first],pl,false);
	}
	outfile<<"endsolid BLK"<<ss.str ()<<endl;
	return 0;
}

double block_system::cal_vol_estimate (const block &b) const
{
	using CGAL::to_double;
	double res(0);
	for (int i(0);i!=b.vfi.size ();++i)
	{
		const point_3 &p1 (vpo[vfa[b.vfi[i].first ].vl[0].vpi[0]]);
		for (int j(0);j!=vfa[b.vfi[i].first].vl.size ();++j)
		{
			for (int k(0);k!=vfa[b.vfi[i].first].vl[j].vei.size ();++k)
			{
				// + - -
				// + + +
				// - + -
				// - - +
				edge e (ved[vfa[b.vfi[i].first].vl[j].vei[k]]);
				if (b.vfi[i].second != vfa[b.vfi[i].first].vl[j].ve_ori[k])
					e = e.opposite();
				const point_3 &p2 (vpo[e.n[0]]), &p3 (vpo[e.n[1]]);
				res += to_double (p1.x() * p2.y() * p3.z() + p1.y() * p2.z() * p3.x() + p1.z() * p2.x() * p3.y()
					-p1.z() * p2.y() * p3.x() - p1.y() * p2.x() * p3.z() - p1.x() * p2.z() * p3.y());
			}
		}
	}
	return res/6.0;
}

FT block_system::cal_vol (const block &b) const
{
	FT res(0);
	for (int i(0);i!=b.vfi.size ();++i)
	{
		const point_3 &p1 (vpo[vfa[b.vfi[i].first ].vl[0].vpi[0]]);
		for (int j(0);j!=vfa[b.vfi[i].first].vl.size ();++j)
		{
			for (int k(0);k!=vfa[b.vfi[i].first].vl[j].vei.size ();++k)
			{
				// + - -
				// + + +
				// - + -
				// - - +
				edge e (ved[vfa[b.vfi[i].first].vl[j].vei[k]]);
				if (b.vfi[i].second != vfa[b.vfi[i].first].vl[j].ve_ori[k])
					e = e.opposite();
				const point_3 &p2 (vpo[e.n[0]]), &p3 (vpo[e.n[1]]);
				res += (p1.x() * p2.y() * p3.z() + p1.y() * p2.z() * p3.x() + p1.z() * p2.x() * p3.y()
					-p1.z() * p2.y() * p3.x() - p1.y() * p2.x() * p3.z() - p1.x() * p2.z() * p3.y());
			}
		}
	}
	return res/6.0;
}

int block_system::gen_loop (const int& i, std::vector <loop> &vl)
{
	//vector<loop> vl_edge;	//����ÿ��loop����¼�����ved�ϵ�������vl_edge[i].vpi[i]��¼���Ǳ�(vl[i].vpi[i], vl[i].vpi[i+1])		����ʱû���õ���
	//vector<vector<bool>> vl_edge_ori;		//��¼ÿ��loop�ı߷���true�������loop�е�����ߺ�vl_edge��������Ӧ�ı߷�����ͬ��false�������෴	����ʱû���õ���

	vector<int> vet (vpl_edi[i].size (),0);		//ÿ��edge����Щ�Ѿ���������0:����û�б���ӣ�1����������ӣ�2�����������,3:���Ѿ������
	int count(0);			//�Ѿ��ж��ٱ߱������
	bool tracing (false);		//�Ƿ��������һ��loop�Ĺ�����
	int vpl_edi_size ((int)vpl_edi[i].size ());

	while (count < 2*vpl_edi_size)
	{
		if (!tracing)			//Ҫ��ʼһ���µ�loop
		{
			vl.push_back (loop());

			for (int j(0);j!=vpl_edi_size;++j)
			{
				bool temp(false);
				switch (vet[j])		//��һ���߼�����ʼ
				{
				case 0:				//�����û�б���ǹ�
					temp = true;
					vl.back().vpi.push_back (ved[vpl_edi[i][j]].n[0]);
					vl.back ().vpi.push_back (ved[vpl_edi[i][j]].n[1]);
					vl.back ().vei.push_back ((int)vpl_edi[i][j]);
					vl.back ().ve_ori.push_back (true);
					vet[j] = 1; count++; tracing= true;
					break;
				case 1:				//��������򱻱�ǹ�
					temp = true;
					vl.back ().vpi.push_back (ved[vpl_edi[i][j]].n[1]);
					vl.back ().vpi .push_back (ved[vpl_edi[i][j]].n[0]);
					vl.back ().vei.push_back ((int)vpl_edi[i][j]);
					vl.back ().ve_ori.push_back (false);
					vet[j] = 3; count++; tracing = true;
					break;
				case 2:				//����߷��򱻱�ǹ�
					temp = true;
					vl.back().vpi.push_back (ved[vpl_edi[i][j]].n[0]);
					vl.back ().vpi.push_back (ved[vpl_edi[i][j]].n[1]);
					vl.back ().vei.push_back ((int)vpl_edi[i][j]);
					vl.back ().ve_ori.push_back (true);
					vet[j] = 3; count++; tracing= true;
					break;
				default:
					break;
				}
				if (temp)
					break;
			}
		}
		else
		{								//�������һ��loop�Ĺ�����,���δ��ɵ�loop��vl.back()
			vector<int> ve_cand;		//��ѡ�ߵļ���,��¼����vpl_edi[i]�е�Ԫ�ص�����
			vector<bool> ve_dir;
			ve_cand.reserve (10);
			ve_dir.reserve (10);
			for (int j(0);j!=vpl_edi_size;++j)
			{
				if (vet[j] == 3) continue;		//������Ѿ����ù���
				if ((vl.back ().vpi.back() == ved[vpl_edi[i][j]].n[0]) && (vet[j] == 0 || vet[j] == 2))
				{
					ve_cand.push_back (j); ve_dir.push_back (true);		//����û����ӵ�ʱ��
				}

				if ((vl.back ().vpi.back() == ved[vpl_edi[i][j]].n[1]) && (vet[j] == 0 || vet[j] == 1))
				{
					ve_cand.push_back (j); ve_dir.push_back (false);
				}
			}
			if (ve_cand.empty())
				throw logic_error ("identify(), face tracing, no candiate edge");
			int maxi (0);		//��¼���ҵı���ve_cand�е�����
			edge e_prev (edge(vl.back ().vpi[vl.back ().vpi.size ()-2], vl.back().vpi.back ()));
			edge e_next (ved[vpl_edi[i][ve_cand[maxi]]]);		//���ҵı�
			if (!ve_dir[maxi]) e_next = e_next.opposite();

			right_most_compare_3 compare (vpl[i],vpo,e_prev);
			for (int j(0);j!=ve_cand.size ();++j)
			{
				if (ve_dir[j])		//���ڴ���һ�������
				{
					if (compare (e_next,ved[vpl_edi[i][ve_cand[j]]]))
					{
						maxi = j; e_next = ved[vpl_edi[i][ve_cand[j]]];
					}
				}
				else			//���ڴ���һ�������
					if (compare (e_next, ved[vpl_edi[i][ve_cand[j]]].opposite()))
					{
						maxi = j; e_next = ved[vpl_edi[i][ve_cand[j]]].opposite();
					}
			}

			switch (vet[ve_cand[maxi]])
			{
			case 0:		//�ҵ������ұ�û�б���ӹ�
				if (ve_dir[maxi])
					vet[ve_cand[maxi]] = 1;
				else
					vet[ve_cand[maxi]] = 2;

				break;
			case 1:		//�ҵ������ұ�������ӹ�
				if (ve_dir[maxi])
					throw logic_error ("identify(), face tracing edge marked again");
				else
					vet[ve_cand[maxi]] = 3;
				break;
			case 2:		//�ҵ������ұ߷�����ӹ�
				if (ve_dir[maxi])
					vet[ve_cand[maxi]] = 3;
				else
					throw logic_error ("identify(), face tracing edge marked again");
				break;
			case 3:
				throw logic_error ("identify(), face tracing edge marked again");
			default:	break;
			}
			count ++;
			if (vl.back ().vpi [0] == e_next.n[1])		//���Ҫ��ӵıߵ������loop�ĵ�һ���ߣ�������
				tracing = false;
			else
				vl.back ().vpi.push_back (e_next.n[1]);

			vl.back ().vei.push_back ((int)vpl_edi[i][ve_cand[maxi]]);
			vl.back ().ve_ori.push_back (ve_dir[maxi]);
		}
		
	}
	if (tracing)
		throw logic_error ("trac_face(), edge is consumed before finishing the loop");

	vector<loop> vl_temp;
	vl_temp.reserve (vl.size ()*5);
	for (int j(0);j!= vl.size ();++j)
		split_loop(vl[j],vl_temp);
	vl.swap (vl_temp);

	return 0;
}

int block_system::loop2face (const int& pli,const std::vector<loop> &vl)
{
	vector<int> vlt (vl.size ());		//ÿ��loop��type,0:�˻��߽磬1��˳ʱ����ת��2����ʱ����ת
	vector<vector<int>> vl_containing (vl.size ());		//ÿ��loop��������loop������

	for (int i(0);i!=vl.size ();++i)
	{
		switch (vl[i].vpi.size ())
		{
		case 0:
		case 1:
			throw logic_error("loop2face, illegal loops"); break;
		case 2:
			vlt[i] = 0; break;
		default:
			if (oriented_normal(vpo,vl[i].vpi) * vpl[pli].orthogonal_vector() > 0)
				vlt[i] = 2;
			else
				vlt[i] = 1;
			break;
		}
	}

	vector<index_l> temp_vl (vl.size ());		//��vl��˳ʱ����ת��loop���ĳ���ʱ���,����ֻ����vpi����
	for (int i(0);i!=vl.size();++i)
	{
		if (vlt[i] == 0)
			continue;
		else if (vlt[i] == 1)
		{
			temp_vl[i] = vl[i].vpi;
			reverse (temp_vl[i].begin (), temp_vl[i].end ());
		}
		else 
			temp_vl[i] = vl[i].vpi;
	}

	//�ж�loop֮��İ�����ϵ
	for (int i(0);i!=vl.size ();++i)
	{
		if (vlt[i] == 0)
			continue;

		for (int j(0);j!=vl.size ();++j)
		{
			if (i==j) continue;
			if (vlt[j] != 0)
			{
				//���������loop��ȾͲ���Ϊ�ǰ�����ϵ���αػ����˺�
				if ((temp_vl[i] != temp_vl[j]) && (polygon_in_polygon_3(vpo,temp_vl[i],temp_vl[j],vpl[pli]) == 1))
					vl_containing[i].push_back (j);
			}
			else
			{
				switch (point_in_polygon_3(vpo,temp_vl[i],vpl[pli],CGAL::midpoint(vpo[vl[j].vpi[0]], vpo[vl[j].vpi[1]])))
				{
				case 0:
					throw logic_error ("loop2face(), edge on loop"); break;
				case 1:
					vl_containing[i].push_back (j);
				} 
			}
		}
	}

	vector<int> vl_fai (vl.size (),-1);		//��¼vl��ÿ��outer loop����ӵ��ĸ�face����
	for (int i(0);i!=vl.size ();++i)
		if (vlt[i] == 2)
		{
			for (int j(0);j<vl_containing[i].size ();++j)
			{
				int temp_li (vl_containing[i][j]);
				if (vlt[temp_li] == 2)
				{
					remove_set (vl_containing[i],vl_containing[temp_li]);
					remove_set (vl_containing[i],temp_li);
					--j;
				}
			}
		}

	vector<face> temp_vfa;
	temp_vfa.reserve (vl.size ());
	for (int i(0);i!=vl.size ();++i)
	{
		if (vlt[i] == 2)
		{
			temp_vfa.push_back(face());
			temp_vfa.back ().vl.push_back (vl[i]);
			temp_vfa.back ().pli = pli;
			if (vl_fai[i] == -1)
				vl_fai[i] = (int)temp_vfa.size ()-1;
			else
				throw logic_error ("loop2face, loop has been added");
			for (int j(0);j!=vl_containing[i].size ();++j)
			{
				temp_vfa.back ().vl.push_back (vl[vl_containing[i][j]]);
				if (vl_fai[vl_containing[i][j]] == -1)
					vl_fai[vl_containing[i][j]] = (int)temp_vfa.size ()-1;
				else
					throw logic_error ("loop2face, loop has been added");
			}
		}
	}

	for (int i(0);i!=temp_vfa.size ();++i)
	{
		point_3 temp (get_point_in_face_3(vpo,vpl[pli],temp_vfa[i]));
		//������face�ϵĵ㼴�ں�����ϣ�Ҳ����϶����
		if ((point_in_polygon_3(vsec[pli].vp,vsec[pli].vl,vpl[pli],temp) != 2)
			&& (point_in_polygon_3(vsec_pf[pli].vp,vsec_pf[pli].vl,vpl[pli],temp) !=2))
			vfa.push_back (temp_vfa[i]);
	}
	return 0;
}

int block_system::gen_edges ()
{
	std::vector <segment_3> vseg;
	std::vector <point_3> vtempp;
	std::vector <bbox_3> vbox_pf;		//bbox for each poly frac
	std::vector <bbox_3> vbox_seg;		//bbox for each segment in vseg
	std::vector<int> vseg_type;			//whether the segment in vseg is in the domain
										//0: Not judged yet. 1:inside. 2:outside

	CGAL::Timer timer;
	cout<<"Initializing bbox ";
	timer.start ();
	//Initialize bbox for each poly fracture
	vbox_pf.reserve (vpf.size ());
	for (int i(0);i!=vpf.size ();++i)
		vbox_pf.push_back (vpf[i].init_bbox());

	vseg.reserve (domain.vt.size ()*3 + vpf.size ()*30);
	vbox_seg.reserve (domain.vt.size ()*3 + vpf.size ()*30);
	for (int i(0);i!=domain.vt.size();++i)
	{
		add_set_seg_3(vseg,segment_3 (domain.vp[domain.vt[i].n[0]],domain.vp[domain.vt[i].n[1]]),vbox_seg);
		add_set_seg_3(vseg,segment_3 (domain.vp[domain.vt[i].n[0]],domain.vp[domain.vt[i].n[2]]),vbox_seg);
		add_set_seg_3(vseg,segment_3 (domain.vp[domain.vt[i].n[2]],domain.vp[domain.vt[i].n[1]]),vbox_seg);
	}

	for (int i(0);i!=vpf.size ();++i)
		for (int j(0);j!=vpf[i].vl.size ();++j)
			for (int k(0);k!=vpf[i].vl[j].size ();++k)
			{
				int k1 (k+1);
				if (k1 >= vpf[i].vl[j].size ())
					k1 = 0;
				add_set_seg_3 (vseg, segment_3 (vpf[i].vl[j][k], vpf[i].vl[j][k1]), vbox_seg);
			}

	timer.stop ();
	cout<<timer.time ()<<endl;

	timer.reset ();
	timer.start ();
	cout<<"Generating all the intersected segments "; 
	//�������еĽ��߶�
	for (int i(0);i!= vpf.size ();++i)
	{
		cout<<i<<endl;
		for (int j (i+1); j<vpf.size ();++j)
		{
			if (!CGAL::do_overlap (vbox_pf[i],vbox_pf[j]))
				continue;
			intersection(vpf[i], vpl[vpf[i].plane_id ],
				vpf[j], vpl[vpf[j].plane_id],
				vseg,vtempp,vbox_seg);
		}
	}
	
	for (int i(0);i!= domain.vt.size ();++i)
	{
		cout<<i<<"/"<<domain.vt.size ()<<endl;
		plane_3 temp_pl(vpl[domain.vpli[i]]);
		if (!domain.vtri_ori[i])
			temp_pl = temp_pl.opposite();
		poly_frac pf;

		bbox_3 df_bbox (domain.init_bbox (i));
		
		pf.vl.push_back (poly_frac::f_loop());
		pf.vl.back ().push_back (domain.vp[domain.vt[i].n[0]]);
		pf.vl.back ().push_back (domain.vp[domain.vt[i].n[1]]);
		pf.vl.back ().push_back (domain.vp[domain.vt[i].n[2]]);
		for (int j(0);j!=vpf.size ();++j)
		{
			if (!CGAL::do_overlap(df_bbox,vbox_pf[j]))
				continue;
			intersection (pf,temp_pl,vpf[j],vpl[vpf[j].plane_id],
						vseg,vtempp,vbox_seg);
		}
	}
	
	//��vtempp�еĵ���Ѹ����߶�
	sort(vtempp.begin (),vtempp.end (), point_compare_3());
	vtempp.resize (unique (vtempp.begin (),vtempp.end ()) - vtempp.begin ());
	for (int i(0);i!=vtempp.size ();++i)
		split_seg(vseg, vbox_seg, vtempp[i]);

	timer.stop ();
	cout<<timer.time ()<<endl;
	timer.reset ();
	
	

	//�޳����о�����������߶�
	timer.start ();
	cout<<"Deleting segments out of domain ";
	vseg_type.resize (vseg.size (),0);					//ÿ���߶��Ƿ�Ӧ�ý�����һ���ķ���,0:��û���жϣ�1��Ҫ���룬2����Ҫ����
	vector<int> vpl_seg_count (vpl.size (),0);			//ÿ��ƽ�����ж����߶��Ѿ����жϹ���
	vector<vector<size_t>> vseg_pli (vseg.size ());		//ÿ���߶�����Щƽ����
	vector<vector<size_t>> vpl_segi (vpl.size ());		//ÿ��ƽ���ϵ��߶�
	
	//vector<vector<size_t>> vpl_domainti (vpl.size ());	//ÿ��ƽ�����ж���domain��������
	//for (int i(0);i!=domain.vpli.size ();++i)
	//{
	//	vpl_domainti[domain.vpli[i]].push_back (i);
	//}

	for (int i(0);i!=vseg.size ();++i)
	{
		for (int j(0);j!=vpl.size ();++j)
		{
			if (vpl[j].has_on(vseg[i].target()) && vpl[j].has_on(vseg[i].source ()))
			{
				vseg_pli[i].push_back (j);
				vpl_segi[j].push_back (i);
			}
		}
	}

	for (int i(0);i!=vpl.size ();++i)
	{
		cout<<i<<endl;

		if (vpl_seg_count[i] == vpl_segi[i].size ())
			continue;		//���ÿһ���߶ζ����ж���ֱ�Ӽ���

		for (int j(0);j!=vpl_segi[i].size ();++j)
		{
			if (vseg_type[vpl_segi[i][j]] != 0)
				continue;		//����߶��Ѿ����жϹ���
			vseg_type[vpl_segi[i][j]] = 1;	//Ĭ��Ҫ����
			vpl_seg_count[i] ++;
			
			point_3 mid(CGAL::midpoint(vseg[vpl_segi[i][j]].source (), vseg[vpl_segi[i][j]].target ()));
			if (point_in_polygon_3(vsec[i].vp,vsec[i].vl,vpl[i],mid) == 2)
				vseg_type [vpl_segi[i][j]] = 2;
		}
	}
	timer.stop ();
	cout<<timer.time()<<endl;
	timer.reset ();

	//���������о������ڵı߶���ӵ�ved��
	timer.start ();
	cout<<"Generating ved ";
	vpl_edi.resize (vpl.size ());
	ved_pli.reserve (vseg.size ());
	for (int i(0);i!=vseg.size ();++i)
	{
		switch (vseg_type[i])
		{
		case 0:
			throw logic_error ("gen_edges, edge unmarked"); break;
		case 1:
			ved.push_back (edge(add_set (vpo,vseg[i].source ()), add_set(vpo,vseg[i].target ())));
			ved.back ().sort ();
			ved_pli.push_back (vseg_pli[i]);
			for (int j(0);j!=vseg_pli[i].size ();++j)
				vpl_edi[vseg_pli[i][j]].push_back ((int)ved.size ()-1);
			break;
		default:
			break;
		}
	}
	timer.stop ();
	cout<<timer.time ()<<endl;
	return 0;
}

int block_system::init_csections (const int& pli, csection &sec)
{
	//�����Ż�һ�µģ����ֱ�Ӽ������еĽ���Ļ�ֻ��Ҫ����һ��domain.vt�Ϳ�����
	sec.clear ();
	domain.plane_intersect(vpl[pli],sec);
	for (int i(0);i!=domain.vt.size ();++i)
	{
		if (domain.vpli[i] != pli)
			continue;
		sec.vl.push_back (index_l());
		sec.vp.push_back (domain.vp[domain.vt[i].n[0]]);
		sec.vp.push_back (domain.vp[domain.vt[i].n[1]]);
		sec.vp.push_back (domain.vp[domain.vt[i].n[2]]);
		if (domain.vtri_ori[i])
		{
			sec.vl.back ().push_back ((int)sec.vp.size () - 3);
			sec.vl.back ().push_back ((int)sec.vp.size () - 2);
			sec.vl.back ().push_back ((int)sec.vp.size () - 1);
		}
		else
		{
			sec.vl.back ().push_back ((int)sec.vp.size () - 1);
			sec.vl.back ().push_back ((int)sec.vp.size () - 2);
			sec.vl.back ().push_back ((int)sec.vp.size () - 3);
		}
	}
	return 0;
}

int block_system::init_vsec_pf ()
{
	std::vector <std::vector <size_t>> vpl_dfai;		//��¼ÿ��ƽ��������Щdomain������������
	std::vector <std::vector <size_t>> vpl_pfi;			//��¼ÿ��ƽ��������Щ�������϶
	
	vsec_pf.resize (vpl.size ());
	vpl_dfai.resize (vpl.size ());
	vpl_pfi.resize (vpl.size ());

	for (int i(0);i!=domain.vt.size ();++i)
		vpl_dfai[domain.vpli[i]].push_back (i);
	for (int i(0);i!=vpf.size ();++i)
		vpl_pfi[vpf[i].plane_id].push_back (i);

	for (int i(0);i!=vpl.size ();++i)
	{
		vsec_pf[i].clear ();
		for (int j(0);j!=vpl_dfai[i].size ();++j)
		{
			vsec_pf[i].vl.push_back (index_l());
			vsec_pf[i].vp.push_back (domain.vp[domain.vt[vpl_dfai[i][j]].n[0]]);
			vsec_pf[i].vp.push_back (domain.vp[domain.vt[vpl_dfai[i][j]].n[1]]);
			vsec_pf[i].vp.push_back (domain.vp[domain.vt[vpl_dfai[i][j]].n[2]]);
			if (domain.vtri_ori[vpl_dfai[i][j]])
			{
				vsec_pf[i].vl.back ().push_back ((int) vsec_pf[i].vp.size ()-3);
				vsec_pf[i].vl.back ().push_back ((int) vsec_pf[i].vp.size ()-2);
				vsec_pf[i].vl.back ().push_back ((int) vsec_pf[i].vp.size ()-1);
			}
			else
			{
				vsec_pf[i].vl.back ().push_back ((int) vsec_pf[i].vp.size ()-1);
				vsec_pf[i].vl.back ().push_back ((int) vsec_pf[i].vp.size ()-2);
				vsec_pf[i].vl.back ().push_back ((int) vsec_pf[i].vp.size ()-3);
			}
		}
		for (int j(0);j!=vpl_pfi[i].size ();++j)
			for (int k(0);k!= vpf[vpl_pfi[i][j]].vl.size ();++k)
			{
				vsec_pf[i].vl.push_back (index_l());
				for (int m(0);m!=vpf[vpl_pfi[i][j]].vl[k].size ();++m)
				{
					vsec_pf[i].vl.back ().push_back ((int) vsec_pf[i].vp.size ());
					vsec_pf[i].vp.push_back (vpf[vpl_pfi[i][j]].vl[k][m]);
				}
			}
	}
	return 0;
}

int block_system::check_face_loop_edge () const
{
	std::vector <std::vector<size_t>> temp_ved_pli(ved.size ());		//ÿ��edge����Щƽ����
	std::vector <std::vector<size_t>> temp_vpl_edi(vpl.size ());		//ÿ��ƽ��������Щedge
	//���ved
	for (int i(0);i!=ved.size ();++i)
	{
		if (ved[i].n[0] >= ved[i].n[1])
			return 1;
		for (int j(0);j!=vpl.size ();++j)
			if (vpl[j].has_on (vpo[ved[i].n[0]]) && vpl[j].has_on (vpo[ved[i].n[1]]))
			{
				temp_ved_pli[i].push_back (j);
				temp_vpl_edi[j].push_back (i);
			}
	}

	//���ved_pli
	for (int i(0);i!=ved.size ();++i)
	{
		//ved_pli[i] is included in temp_ved_pli[i]
		for (int j(0);j!=ved_pli[i].size ();++j)
			if (find (temp_ved_pli[i].begin (), temp_ved_pli[i].end (),ved_pli[i][j]) == temp_ved_pli[i].end ())
				return 2;

		for (int j(0);j!=temp_ved_pli[i].size ();++j)
			if (find (ved_pli[i].begin (), ved_pli[i].end (),temp_ved_pli[i][j]) == ved_pli[i].end ())
				return 3;
	}

	//���vpl_edi
	for (int i(0);i!=vpl.size ();++i)
	{

		for (int j(0);j!=vpl_edi[i].size ();++j)
			if (find (temp_vpl_edi[i].begin (), temp_vpl_edi[i].end (),vpl_edi[i][j]) == temp_vpl_edi[i].end ())
				return 4;

		for (int j(0);j!=temp_vpl_edi[i].size ();++j)
			if (find (vpl_edi[i].begin (), vpl_edi[i].end (),temp_vpl_edi[i][j]) == vpl_edi[i].end ())
				return 5;
	}

	//������face
	for (int i(0);i!=vfa.size ();++i)
	{
		for (int j(0);j!=vfa[i].vl.size ();++j)
		{
			const loop & l = vfa[i].vl[j];
			if (l.vpi.size () != l.vei.size ())
				return 6;
			if (l.vpi.size () != l.ve_ori.size ())
				return 7;
			for (int k(0);k!=l.vpi.size ();++k)
			{
				int k1 (k+1);
				if (k1 >= l.vpi.size ()) k1=0;
				if (ved[l.vei[k]] != edge(l.vpi[k], l.vpi[k1]))
					return 8;
				if (l.ve_ori[k] && (ved[l.vei[k]].n[0] != l.vpi[k]))
					return 9;
				if ((!l.ve_ori[k]) && (ved[l.vei[k]].n[0] != l.vpi[k1]))
					return 10;
				if (find (ved_pli[l.vei[k]].begin (),ved_pli[l.vei[k]].end (), vfa[i].pli) == ved_pli[l.vei[k]].end ())
					return 11;		//���������û�а�����������edge��
			}
		}
	}
	return 0;
}

int block_system::gen_blk ()
{
	vector<vector<pair<int,bool>>> ved_fai(ved.size ());			//ÿ��edge���ڵ�ƽ�������͸�edge�����ƽ��ĳ���

	for (int i(0);i!=vfa.size ();++i)
		for (int j(0);j!=vfa[i].vl.size ();++j)
		{
			const loop & l = vfa[i].vl[j];
			for (int k(0);k!=l.vei.size ();++k)
				ved_fai[l.vei[k]].push_back (pair<int,bool>(i,l.ve_ori[k]));
		}

	vector<int> vft (vfa.size ());		//ÿ��face����Щ�Ѿ��������ˣ�0������û�б���ӣ�1����������ӣ�2�����������,3:���Ѿ������
	int count(0);						//�ж���face�Ѿ��������
	bool tracing (false);
	int vfa_size ((int) vfa.size ());

	while (count < 2 * vfa_size)
	{
		if (!tracing)			//Ҫ��ʼһ���µĿ���
		{
			vblk.push_back (block());

			for (int i(0);i!=vfa_size;++i)
			{
				bool temp (false);
				switch (vft[i])
				{
				case 0:
					temp = true;
					vblk.back ().vfi.push_back (pair<int,bool> (i,true));
					tracing = true;
					break;
				case 1:
					temp = true;
					vblk.back ().vfi.push_back (pair<int,bool> (i,false));
					tracing = true;
					break;
				case 2:
					temp = true;
					vblk.back ().vfi.push_back (pair<int,bool> (i,true));
					tracing = true;
					break;
				default:
					break;
				}
				if (temp)
					break;
			}
		}
		else					//�������һ������
		{
			for (int i(0);i!=vblk.back ().vfi.size ();++i)
			{
				vector_3 fa_n (vpl[vfa[vblk.back ().vfi[i].first].pli].orthogonal_vector());	//��ǰface�ķ�������
				if (!vblk.back ().vfi[i].second)
					fa_n = -fa_n;
				for (int j(0);j!=vfa[vblk.back ().vfi[i].first].vl.size ();++j)
				{
					const loop & l(vfa[vblk.back().vfi[i].first].vl[j]);
					for (int k(0);k!=l.vei.size ();++k)
					{
						//�����face�а�����ÿ��edge
						vector<pair<int,bool>> cand_fa;			//��¼�����ǰedge��twin���ڵĺ�ѡ��
						bool true_ori_curr (!(l.ve_ori[k] ^ vblk.back ().vfi[i].second));		//��ǰedge���������������ǰ���Ƿ������ģ��������е�edge��Ӧ�÷�����
						for (int m(0);m!=ved_fai[l.vei[k]].size ();++m)
						{
							//if (ved_fai[l.vei[k]][m].first == vblk.back ().vfi[i].first)	//��ǰ�������������ͬһ����
							//{
							//	switch (vft[ved_fai[l.vei[k]][m].first])		//�����ǰ���twin��û�б���Ŀ����õ��������Ϊ��ѡ��
							//	{
							//	case 0:
							//		cand_fa.push_back (pair<int,bool>(ved_fai[l.vei[k]][m].first, !vblk.back ().vfi[i].second ));
							//		break;		//ֻ�������涼û�б��õ���ʱ��ſ��ܱ����
							//	case 1:
							//		if (vblk.back ().vfi[i].second)		//��ǰ���������ƽ��ͬ�򣬵���ʱ�����ѱ�ʹ�ã��򲻿���
							//			throw logic_error("gen_blk, positive face has been used in other blocks");
							//		break;
							//	case 2:
							//		if (!vblk.back ().vfi[i].second )	//��ǰ���������ƽ�淴�򣬵���ʱ�����ѱ�ʹ�ã��򲻿���
							//			throw logic_error("gen_blk, negative face has been used in other blocks");
							//		break;
							//	default:
							//		throw logic_error ("gen_blk, face has been used in other blocks");		//�����涼�����ˣ������������Ͳ��ñ����
							//		break;
							//	}
							//	continue;
							//}

							switch (vft[ved_fai[l.vei[k]][m].first])
							{
							case 0:		//�����������߶�û�б�����
								if (true_ori_curr == ved_fai[l.vei[k]][m].second )		
									cand_fa.push_back (pair<int,bool> (ved_fai[l.vei[k]][m].first ,false));//��ǰ�ߵ�twin�ڴ�������ķ���
								else
									cand_fa.push_back (pair<int,bool> (ved_fai[l.vei[k]][m].first ,true));//��ǰ�ߵ�twin�ڴ������������
								break;
							case 1:		//��������������Ѿ������ã����������䷴��
								if (true_ori_curr == ved_fai[l.vei[k]][m].second)
									cand_fa.push_back (pair<int,bool> (ved_fai[l.vei[k]][m].first ,false));//��ǰ�ߵ�twin�ڴ�������ķ���
								break;
							case 2:		//��������ķ����Ѿ������ã���������������
								if (true_ori_curr != ved_fai[l.vei[k]][m].second)
									cand_fa.push_back (pair<int,bool> (ved_fai[l.vei[k]][m].first ,true));//��ǰ�ߵ�twin�ڴ������������
							default:
								break;
							}
						}
						if (cand_fa.empty())
						{
							//if (l.vpi.size () <=2)		//���loop���˻���loop������������������ԭ�����ܱ������߽�������
							//	continue;
							//else
								throw logic_error ("gen_blk(), no candidate face");
						}

						//Ѱ�Һ�ѡƽ��
						vector<vector_3> cand_dir;				//��¼��ѡ�������淨��
						cand_dir.reserve (cand_fa.size ());
						for (int m(0);m!=cand_fa.size ();++m)
						{
							cand_dir.push_back (vpl[vfa[cand_fa[m].first].pli].orthogonal_vector());
							if (!cand_fa[m].second)
								cand_dir.back () = - cand_dir.back ();
						}
						vector_3 ev (vector_3 (vpo[ved[l.vei[k]].n[0]], vpo[ved[l.vei[k]].n[1]]));		//��ǰ�ߵķ�������
						if (!true_ori_curr)
							ev = -ev;
						right_most_compare_3_vector compare(ev,fa_n);
						int maxi(0);
						for (int m(0);m!=cand_fa.size ();++m)
							if (compare (cand_dir[maxi], cand_dir[m]))
								maxi = m;

						//�ҵ��˺�ѡƽ��,ֱ����ӵ������У��ظ���ֱ�Ӻ���
						add_set (vblk.back ().vfi,cand_fa[maxi]);
					}
				}
			}

			//�����������������
			tracing = false;
			count += (int) vblk.back ().vfi.size ();
			for (int i(0);i!=vblk.back ().vfi.size ();++i)
			{
				switch (vft[vblk.back ().vfi[i].first])
				{
				case 0:
					if (vblk.back ().vfi[i].second )
						vft[vblk.back ().vfi[i].first ] = 1;
					else
						vft[vblk.back ().vfi[i].first ] = 2;
					break;
				case 1:
					if (vblk.back ().vfi[i].second )
						throw logic_error ("gen_blk(), face already used");
					else
						vft[vblk.back ().vfi[i].first ] = 3;
					break;
				case 2:
					if (vblk.back ().vfi[i].second )
						vft[vblk.back ().vfi[i].first ] = 3;
					else
						throw logic_error ("gen_blk(), face already used");
					break;
				default:
					break;
				}
			}

		}
	}
	if (tracing)
		throw logic_error ("gen_blk(), face is consumed befor finishing the blocks");
	
	//�����������е��˻���������
	int vblk_size ((int)vblk.size ());
	for (int i(0);i!=vblk_size;++i)
	{
		if (vblk[i].vfi.size () <2)
			throw logic_error ("gen_blk, block contain less than 2 faces");
		if (vblk[i].vfi.size () == 2)
			if (vblk[i].vfi[0].first != vblk[i].vfi[1].first)
				throw logic_error ("gen_blk, illegal blk");
		vector<int> v_flag (vblk[i].vfi.size (),0);		//����Ƿ����
		int max (1);
		
		for (int j(0);j!=vblk[i].vfi.size ();++j)
		{
			if (v_flag[j] != 0)
				continue;		//�Ѿ���Թ���
			int k(j+1);
			for (;k< vblk[i].vfi.size ();++k)
				if (vblk[i].vfi[j].first == vblk[i].vfi[k].first )
					break;
			if (k < vblk[i].vfi.size ())	//�ҵ������
			{
				v_flag[j] = v_flag[k] = ++max;
				vblk.push_back (block());
				vblk.back ().vfi.push_back (vblk[i].vfi[j]);
				vblk.back ().vfi.push_back (vblk[i].vfi[k]);
			}
		}
		if (max == 1)
			continue;		//û���˻��߽�
		block temp_b;
		temp_b.vfi .reserve (vblk[i].vfi .size ());
		for (int j(0);j!=vblk[i].vfi.size ();++j)
		{
			if (v_flag[j] != 0)
				continue;
			temp_b.vfi.push_back (vblk[i].vfi[j]);
		}
		if (temp_b.vfi.size () == 0)
		{
			vblk[i] = vblk.back ();
			vblk.pop_back();
		}
		else if (temp_b.vfi.size () < 4)
			throw logic_error ("gen_blk, blk contain less than 4 faces");
		else
			vblk[i] = temp_b;
	}
	return 0;
}

int block_system::extract_outer_surfaces ()
{
	vfa_ext.clear ();
	vblk_ext.clear ();
	vblk_int.clear ();

	vfa_ext.reserve (vfa.size ());
	vblk_ext.reserve (vblk.size ());
	vblk_int.reserve (vblk.size ());
	
	for (int i(0);i!=vblk.size ();++i)
	{
		block &b (vblk[i]);
		switch (b.vfi.size ())
		{
		case 0:
		case 1:
		case 3:	//��Щ����²�����Χ��һ������
			throw logic_error("block_system::extract_outer_surfaces, illegal blks");
			break;
		case 2:
			continue;
		default:
			//�����������b�а����ĸ����ϵ���
			vblk_ext.push_back (block());
			for (int j(0);j!=b.vfi.size ();++j)
			{
				vfa_ext.push_back (vfa[b.vfi[j].first]);
				vblk_ext.back ().vfi.push_back (pair<int,bool> ((int)vfa_ext.size ()-1, b.vfi[j].second));
			}
			break;
		}
	}
}

int block_system::gen_outer_boundary ()
{
	//ȷ����������İ�����ϵ
	vector<FT> v_vol (vblk.size ());
	vector<point_3> v_p (vblk.size ());		//��¼һ������������ڵĵ�
	vector<vector<bbox_3>> vvbox(vblk.size ());
	for (int i(0);i!=vblk.size ();++i)
	{
		v_vol[i] = cal_vol(vblk[i]);
		get_vbox_of_blk(vpo,vfa,vblk[i],vvbox[i]);
	}

	vblk_ext.clear ();
	vblk_ext.reserve (vblk.size ());
	vector<vector<int>> v_blk_contain (vblk.size ());	//��¼ÿ������������ٸ���������
	for (int i(0);i!=vblk.size ();++i)
	{
		if (v_vol[i] <= 0)
			continue;
		vblk_ext.push_back (vblk[i]);		//ÿ���������0�Ķ�Ӧ����ĳ������������
		for (int j(0); j != vblk.size ();++j)
		{
			if (i == j || abs(v_vol[i]) == abs(v_vol[j]))		//�����ȵĿ��岻����˭����˭
				continue;
			if (find (v_blk_contain[j].begin (), v_blk_contain[j].end (),i) != v_blk_contain[j].end ())
				continue;		//����i�Ѿ������ڿ���j����
			if (v_vol[j] != 0)
			{
				if (polyhedron_in_polyhedron_3(vpo,vfa,vpl,vblk[i],vblk[j],v_vol[i],v_vol[j],vvbox[i],vvbox[j]) == 1)
					v_blk_contain[i].push_back (j);
			}
			else
			{
				//TODO blk_j�����Ϊ0���������ô����
			}
		}
	}


	//TODO 2016-11-14
	


}

}