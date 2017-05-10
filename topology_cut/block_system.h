#pragma once
#include <vector>
#include "geometry.h"
#include <fstream>

namespace TC
{

class block_system
{
public:
	std::vector<point_3> vpo;
	std::vector <plane_3> vpl;
	std::vector <face> vfa;			//ԭʼ�߽磬δ����֮ǰ
	std::vector <block> vblk;		//ԭʼ���壬δ����֮ǰ

	std::vector <face> vfa_ext;		//����������face
	std::vector <block> vblk_ext;	//ÿ�����������棬���е�������Ӧ����vfa_ext�е���
	std::vector <block> vblk_int;	//ÿ��������ڱ���

	//��¼ԭʼ�����ݣ�����domain����϶
	stl_model domain;				
	std::vector <poly_frac> vpf;
	std::vector <disc_frac> vdf;		//Բ����϶�Ͷ������϶�ֿ�����

public:
	int init_domain (std::ifstream & infile);

	int add_polyf (const poly_frac& pf, const poly_frac::Type &type = poly_frac::Type::Both_side);

	int identify ();

	int disc_frac_parser (std::ifstream &infile, const FT& dx, const int & n = 10);

	int output_ved (std::ofstream &outfile) const;

	int output_fac (std::ofstream &outfile, const int&i) const;

	int output_blk (std::ofstream &outfile, const int &i) const;

	int output_outer_bounds (std::ofstream &outfile, const int &i) const;

	double cal_vol_estimate (const block &b) const;

	FT cal_vol (const block &b) const;
public:
	std::vector <edge> ved;		//ÿ�������ڿ���ϵͳ�ڻ�߽��ϵ�edge
	std::vector <std::vector<size_t>> ved_pli;		//ÿ��edge����Щƽ����
	std::vector <std::vector<size_t>> vpl_edi;		//ÿ��ƽ��������Щedge
	std::vector <csection> vsec;		//��¼ÿ��ƽ��pl��domain�Ľ���
	
	std::vector <csection> vsec_pf;		//��¼ÿ��ƽ��pl������Щ�������϶��domain�߽�

	//���ɸ���ƽ���domian�Ľ��棬�����ڲ��ĵ�ͱ߽��ϵĵ�
	//��Щ��������Ľ����������edge������face��ʱ���õ�
	int init_csections (const int& pli, csection &sec);

	//��ʼ��vsec_pf,�ǵ������ɸ���face֮ǰ����
	int init_vsec_pf ();

	//�������е�edge�����Ҹ�ved��vpo��ֵ
	int gen_edges ();

	//Generate the loops on each plane. 
	//pli: index to the plane
	//vl:  results
	int gen_loop (const int& pli, std::vector<loop> &vl);

	//��loopת��Ϊface������������,vl��split��֮���. pli��ʾ����ƽ�������
	int loop2face (const int& pli, const std::vector<loop> &vl);

	//����ԭʼ���壬Ҳ����δ����֮ǰ�Ŀ��壬����Ӧ��������������
	int gen_blk ();

	//��ȡ����������߽磬Ҳ���Ǹ�vfa_ext, vblk_ext��ֵ
	int extract_outer_surfaces();

	//������ݵ�Э����
	int check_face_loop_edge () const;

	int gen_outer_boundary ();

	int sort_edge ()
	{
		edge_compare ec;
		std::sort (ved.begin (),ved.end (),ec);
	}

};

}