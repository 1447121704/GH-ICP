#include "BinaryFeatureExtraction.h"
#include "utility.h"
#include "StereoBinaryFeature.h"
#include <opencv2\core\types_c.h>
#include <opencv2\core\core_c.h>
#include <pcl\registration\correspondence_rejection.h>
#include <pcl\registration\transformation_estimation_svd.h>
#include <glog/logging.h>
#include <concurrent_vector.h>
#include <ppl.h>

using namespace  std;
using namespace utility;
using namespace pcl;
using namespace Eigen;

bool StereoBinaryFeatureExtractor::computeEigenVectorsByWeightPCA(const pcXYZIPtr & input_cloud,const vector<int> & search_indices,int test_index,
	                                                               Eigen::Vector3f  &principalDirection,
															       Eigen::Vector3f  &middleDirection,
															       Eigen::Vector3f  &normalDirection)
{
	if (search_indices.size() < 3)
		return false;


	double radius;
	radius = sqrt(2.0)*extract_radius_;

	//�������ĵ�����,�Լ�(radius-d)�ĺ�,����radius������뾶,d������㵽��ǰ��ľ���;
	double center_x(0.0), center_y(0.0), center_z(0.0), dis_all(0.0);
	for (size_t it = 0; it < search_indices.size(); ++it)
	{
		center_x += input_cloud->points[search_indices[it]].x;
		center_y += input_cloud->points[search_indices[it]].y;
		center_z += input_cloud->points[search_indices[it]].z;

		dis_all += (radius - Comput3DDistanceBetweenPoints(input_cloud->points[search_indices[it]], input_cloud->points[test_index]));
	}
	center_x /= search_indices.size();
	center_y /= search_indices.size();
	center_z /= search_indices.size();

	//����Э�������
	Eigen::Matrix<float, 3, 3> covariance;
	covariance(0, 0) = 0.0f;
	covariance(0, 1) = 0.0f;
	covariance(0, 2) = 0.0f;

	covariance(1, 0) = 0.0f;
	covariance(1, 1) = 0.0f;
	covariance(1, 2) = 0.0f;

	covariance(2, 0) = 0.0f;
	covariance(2, 1) = 0.0f;
	covariance(2, 2) = 0.0f;


	for (size_t it = 0; it < search_indices.size(); ++it)
	{
		float dis, weight;
		dis = Comput3DDistanceBetweenPoints(input_cloud->points[search_indices[it]], input_cloud->points[test_index]);
		weight = radius - dis;
		covariance(0, 0) += weight*(input_cloud->points[search_indices[it]].x - center_x)*(input_cloud->points[search_indices[it]].x - center_x);
		covariance(0, 1) += weight*(input_cloud->points[search_indices[it]].x - center_x)*(input_cloud->points[search_indices[it]].y - center_y);
		covariance(0, 2) += weight*(input_cloud->points[search_indices[it]].x - center_x)*(input_cloud->points[search_indices[it]].z - center_z);

		covariance(1, 0) += weight*(input_cloud->points[search_indices[it]].y - center_y)*(input_cloud->points[search_indices[it]].x - center_x);
		covariance(1, 1) += weight*(input_cloud->points[search_indices[it]].y - center_y)*(input_cloud->points[search_indices[it]].y - center_y);
		covariance(1, 2) += weight*(input_cloud->points[search_indices[it]].y - center_y)*(input_cloud->points[search_indices[it]].z - center_z);


		covariance(2, 0) += weight*(input_cloud->points[search_indices[it]].z - center_z)*(input_cloud->points[search_indices[it]].x - center_x);
		covariance(2, 1) += weight*(input_cloud->points[search_indices[it]].z - center_z)*(input_cloud->points[search_indices[it]].y - center_y);
		covariance(2, 2) += weight*(input_cloud->points[search_indices[it]].z - center_z)*(input_cloud->points[search_indices[it]].z - center_z);

	}
	covariance(0, 0) /= dis_all;
	covariance(0, 1) /= dis_all;
	covariance(0, 2) /= dis_all;

	covariance(1, 0) /= dis_all;
	covariance(1, 1) /= dis_all;
	covariance(1, 2) /= dis_all;

	covariance(2, 0) /= dis_all;
	covariance(2, 1) /= dis_all;
	covariance(2, 2) /= dis_all;

	//������ֵ
	Eigen::EigenSolver<Eigen::Matrix3f> es(covariance, true);
	//�������ֵ����������
	Eigen::EigenSolver<Eigen::Matrix3f>::EigenvalueType ev = es.eigenvalues();
	Eigen::EigenSolver<Eigen::Matrix3f>::EigenvectorsType evc = es.eigenvectors();

	if (es.info() == Eigen::Success)
	{
		float MaxEigenValue = ev(0).real();
		float MinEigenValue = ev(0).real();
		unsigned int MaxEigenValueId(0), MinEigenValueId(0);

		for (int i = 0; i < 3; i++)
		{
			if (ev(i).real() > MaxEigenValue)
			{
				MaxEigenValueId = i;
				MaxEigenValue = ev(i).real();
			}

			if (ev(i).real() < MinEigenValue)
			{
				MinEigenValueId = i;
				MinEigenValue = ev(i).real();
			}
		}

		principalDirection.x() = evc.col(MaxEigenValueId).x().real();
		principalDirection.y() = evc.col(MaxEigenValueId).y().real();
		principalDirection.z() = evc.col(MaxEigenValueId).z().real();

		normalDirection.x() = evc.col(MinEigenValueId).x().real();
		normalDirection.y() = evc.col(MinEigenValueId).y().real();
		normalDirection.z() = evc.col(MinEigenValueId).z().real();

		middleDirection = principalDirection.cross(normalDirection);
	}
	else
	{
		cout << "δ����ȷ����õ������ϵ����" << endl;
		return false;
	}

	return true;
}


bool StereoBinaryFeatureExtractor::computeEigenVectorsByPCA(const pcXYZIPtr & input_cloud,const vector<int> & search_indices,int test_index,
															Eigen::Vector3f  &principalDirection,
															Eigen::Vector3f  &middleDirection,
															Eigen::Vector3f  &normalDirection)
{
	if (search_indices.size() < 3)
		return false;

	CvMat* pData = cvCreateMat((int)search_indices.size(), 3, CV_32FC1);
	CvMat* pMean = cvCreateMat(1, 3, CV_32FC1);
	CvMat* pEigVals = cvCreateMat(1, 3, CV_32FC1);
	CvMat* pEigVecs = cvCreateMat(3, 3, CV_32FC1);

	for (size_t i = 0; i < search_indices.size(); ++i)
	{
		cvmSet(pData, (int)i, 0, input_cloud->points[search_indices[i]].x);
		cvmSet(pData, (int)i, 1, input_cloud->points[search_indices[i]].y);
		cvmSet(pData, (int)i, 2, input_cloud->points[search_indices[i]].z);
	}
	cvCalcPCA(pData, pMean, pEigVals, pEigVecs, CV_PCA_DATA_AS_ROW);

	principalDirection.x() = cvmGet(pEigVecs, 0, 0);
	principalDirection.y() = cvmGet(pEigVecs, 0, 1);
	principalDirection.z() = cvmGet(pEigVecs, 0, 2);

	/*middleDirection.x() = cvmGet(pEigVecs, 1, 0);
	middleDirection.y() = cvmGet(pEigVecs, 1, 1);
	middleDirection.z() = cvmGet(pEigVecs, 1, 2);*/

	normalDirection.x() = cvmGet(pEigVecs, 2, 0);
	normalDirection.y() = cvmGet(pEigVecs, 2, 1);
	normalDirection.z() = cvmGet(pEigVecs, 2, 2);


	middleDirection = principalDirection.cross(normalDirection);

	cvReleaseMat(&pEigVecs);
	cvReleaseMat(&pEigVals);
	cvReleaseMat(&pMean);
	cvReleaseMat(&pData);

	return true;
}

//����ֲ�����ϵ�ͳ�������ϵ����ת����;
void StereoBinaryFeatureExtractor::computeTranformationMatrixBetweenCoordinateSystems(const CoordinateSystem & coordinate_src,
																					  const CoordinateSystem & coordinate_traget,
																					  Eigen::Matrix4f  & tragetToSource)
{
	pcXYZ cloud_src, cloud_target;
	pcl::PointXYZ pt;
	//���cloud_src����;
	pt.x = coordinate_src.xAxis.x();
	pt.y = coordinate_src.xAxis.y();
	pt.z = coordinate_src.xAxis.z();
	cloud_src.points.push_back(pt);

	pt.x = coordinate_src.yAxis.x();
	pt.y = coordinate_src.yAxis.y();
	pt.z = coordinate_src.yAxis.z();
	cloud_src.points.push_back(pt);

	pt.x = coordinate_src.zAxis.x();
	pt.y = coordinate_src.zAxis.y();
	pt.z = coordinate_src.zAxis.z();
	cloud_src.points.push_back(pt);

	//���cloud_target����;
	pt.x = coordinate_traget.xAxis.x();
	pt.y = coordinate_traget.xAxis.y();
	pt.z = coordinate_traget.xAxis.z();
	cloud_target.points.push_back(pt);

	pt.x = coordinate_traget.yAxis.x();
	pt.y = coordinate_traget.yAxis.y();
	pt.z = coordinate_traget.yAxis.z();
	cloud_target.points.push_back(pt);

	pt.x = coordinate_traget.zAxis.x();
	pt.y = coordinate_traget.zAxis.y();
	pt.z = coordinate_traget.zAxis.z();
	cloud_target.points.push_back(pt);


	/*������ԵĶ�Ӧ��ϵcorrespondences;*/
	pcl::Correspondences  correspondences;
	pcl::Correspondence   correspondence;
	for (size_t i = 0; i < 3; i++)
	{
		correspondence.index_match = (int)i;
		correspondence.index_query = (int)i;
		correspondences.push_back(correspondence);
	}

	//���ݶ�Ӧ��ϵcorrespondences������תƽ�ƾ���;
	pcl::registration::TransformationEstimationSVD<pcl::PointXYZ, pcl::PointXYZ> trans_est;
	Eigen::Matrix4f trans;
	trans_est.estimateRigidTransformation(cloud_src, cloud_target, correspondences, trans);
	tragetToSource = trans.inverse();
}

//����2D��pca����ֲ�����ֲ�����������;
void StereoBinaryFeatureExtractor::computeEigenVectorsBy2Dpca(const pcXYZIPtr & input_cloud,//�������
															const vector<int> & search_indices, //�ؼ���������������
															int test_index,
															Eigen::Vector3f  &principalDirection)
{
	if (search_indices.size() < 3)
		return;


	double radius;
	radius = sqrt(2.0)*extract_radius_;

	//�������ĵ�����,�Լ�(R-d)�ĺ�,����R������뾶,d������㵽��ǰ��ľ���;
	double center_x(0.0), center_y(0.0), center_z(0.0),dis_all(0.0);
	for (size_t it = 0; it < search_indices.size(); ++it)
	{
		center_x += input_cloud->points[search_indices[it]].x;
		center_y += input_cloud->points[search_indices[it]].y;
		center_z += input_cloud->points[search_indices[it]].z;

		dis_all += (radius-Comput3DDistanceBetweenPoints(input_cloud->points[search_indices[it]], input_cloud->points[test_index]));
	}
	center_x /= search_indices.size();
	center_y /= search_indices.size();
	center_z /= search_indices.size();

	//����Э�������
	Eigen::Matrix<float, 2, 2> covariance;
	covariance(0, 0) = 0.0f;
	covariance(0, 1) = 0.0f;
	covariance(1, 0) = 0.0f;
	covariance(1, 1) = 0.0f;

	
	for (size_t it = 0; it < search_indices.size(); ++it)
	{
		float dis,weight;
		dis = Comput3DDistanceBetweenPoints(input_cloud->points[search_indices[it]], input_cloud->points[test_index]);
		weight = radius - dis;
		covariance(0, 0) += weight*(input_cloud->points[search_indices[it]].x - center_x)*(input_cloud->points[search_indices[it]].x - center_x);
		covariance(0, 1) += weight*(input_cloud->points[search_indices[it]].x - center_x)*(input_cloud->points[search_indices[it]].y - center_y);
		covariance(1, 0) += weight*(input_cloud->points[search_indices[it]].x - center_x)*(input_cloud->points[search_indices[it]].y - center_y);
		covariance(1, 1) += weight*(input_cloud->points[search_indices[it]].y - center_y)*(input_cloud->points[search_indices[it]].y - center_y);
	}
	covariance(0, 0) /= dis_all;
	covariance(0, 1) /= dis_all;
	covariance(1, 0) /= dis_all;
	covariance(1, 1) /= dis_all;
	
	//������ֵ
	Eigen::EigenSolver<Eigen::Matrix2f> es(covariance, true);
	//�������ֵ����������
	Eigen::EigenSolver<Eigen::Matrix2f>::EigenvalueType ev = es.eigenvalues();
	Eigen::EigenSolver<Eigen::Matrix2f>::EigenvectorsType evc = es.eigenvectors();
	
	if (ev(0).real() > ev(1).real())
	{
		principalDirection.x() = evc.col(0).x().real();
		principalDirection.y() = evc.col(0).y().real();
		principalDirection.z() = 0.0f;
	}
	else
	{
		principalDirection.x() = evc.col(1).x().real();
		principalDirection.y() = evc.col(1).y().real();
		principalDirection.z() = 0.0f;
	}

}

/*����ÿ���ؼ���ľֲ�����ϵ��������ΪZ������,������ΪX������,�����任���þֲ�����ϵ�µĵ�ѹ��result_cloud��;*/
bool StereoBinaryFeatureExtractor::computeLocalCoordinateSystem(const pcXYZIPtr & input_cloud,//�������
	                                                            int test_index,    //�ؼ����������
																const vector<int> & search_indices, //�ؼ���������������
																CoordinateSystem &localCoordinateSystem)
{
	//����PCA����ؼ���ֲ�����ֲ�����������;
	//����PCA����ؼ���ֲ�����ֲ�����������;
	Eigen::Vector3f  principalDirectionPca, normalDirectionPca;
	computeEigenVectorsBy2Dpca(input_cloud, search_indices, test_index, principalDirectionPca);
	//�õ��ֲ�����ϵ;
	localCoordinateSystem.zAxis.x() = 0.0f;
	localCoordinateSystem.zAxis.y() = 0.0f;
	localCoordinateSystem.zAxis.z() = 1.0f;

	localCoordinateSystem.xAxis = principalDirectionPca;
	localCoordinateSystem.yAxis = localCoordinateSystem.zAxis.cross(localCoordinateSystem.xAxis);

	localCoordinateSystem.origin.x() = input_cloud->points[test_index].x;
	localCoordinateSystem.origin.y() = input_cloud->points[test_index].y;
	localCoordinateSystem.origin.z() = input_cloud->points[test_index].z;

	//��������ĳ��Ƚ��й�һ��;
	localCoordinateSystem.xAxis.normalize();
	localCoordinateSystem.yAxis.normalize(); 

	return true;
}


void StereoBinaryFeatureExtractor::transformPointCloudToLocalSystem(const pcXYZIPtr & input_cloud,//�������
																	int test_index,    //�ؼ����������
																	const vector<int> & search_indices, //�ؼ���������������  
																	const CoordinateSystem &localCoordinateSystem,
																	pcXYZIPtr & result_cloud)//������Ѿ������ת�����Եĵ���;
{
	//����ֲ�����ϵ����������ϵ��ת������;
	CoordinateSystem  coordinate_scene;
	coordinate_scene.xAxis.x() = 1.0f; coordinate_scene.xAxis.y() = 0.0f; coordinate_scene.xAxis.z() = 0.0f;
	coordinate_scene.yAxis.x() = 0.0f; coordinate_scene.yAxis.y() = 1.0f; coordinate_scene.yAxis.z() = 0.0f;
	coordinate_scene.zAxis.x() = 0.0f; coordinate_scene.zAxis.y() = 0.0f; coordinate_scene.zAxis.z() = 1.0f;


	Eigen::Matrix4f   tragetToSource;
	computeTranformationMatrixBetweenCoordinateSystems(coordinate_scene, localCoordinateSystem, tragetToSource);

	//��search_indices�еĵ��������ת��,�������ϵĵ�ѹ��reuslt_cloud��;
	//�����ڴ�
	pcl::PointCloud<pcl::PointXYZI>::Ptr temp_cloud(new pcl::PointCloud<pcl::PointXYZI>());
	result_cloud->swap(pcl::PointCloud<pcl::PointXYZI>());
	result_cloud->resize(search_indices.size());
	for (int i = 0; i < search_indices.size(); i++)
	{
		pcl::PointXYZI pt;
		pt.x = input_cloud->points[search_indices[i]].x - input_cloud->points[test_index].x;
		pt.y = input_cloud->points[search_indices[i]].y - input_cloud->points[test_index].y;
		pt.z = input_cloud->points[search_indices[i]].z - input_cloud->points[test_index].z;
		pt.intensity = input_cloud->points[search_indices[i]].intensity;

		temp_cloud->points.push_back(pt);
	}

	pcl::transformPointCloud(*temp_cloud, *result_cloud, tragetToSource);
	temp_cloud->swap(pcl::PointCloud<pcl::PointXYZI>());
}




/*���յ��ܶȼ���ĵ��С�������������*/
void StereoBinaryFeatureExtractor::constructCubicGrid(const pcXYZIPtr & rotated_cloud, vector<GridVoxel> & grid)//�������
						                              
{
	 //�����������ñ���
	 pcl::KdTreeFLANN<pcl::PointXY> tree;
	 vector<int> search_indices;
	 vector<float> distances;

	/*�������е���XOY��ͶӰ,���ø�˹�����Ȩ��������ÿ�������еĵ����;*/
	 pcl::PointCloud<pcl::PointXY>::Ptr cloud_xy(new  pcl::PointCloud<pcl::PointXY>);
	 //��3ά����ͶӰ��XOYƽ��;
	for(size_t i=0;i<rotated_cloud->size();i++)
	{
		pcl::PointXY pt;
		pt.x = rotated_cloud->points[i].x;
		pt.y = rotated_cloud->points[i].y;
		cloud_xy->points.push_back(pt);
	}
	tree.setInputCloud(cloud_xy);

	//����ÿһ������,�ҵ��������ĵ�뾶1.5*unit_side_length_��Χ�ڵĵ�,����Щ���Ȩ������������еĵ����;
	for (size_t i = 0; i < voxel_side_num_; i++)
	{
		for (size_t j = 0; j < voxel_side_num_; j++)
		{
			pcl::PointXY pt;
			float derta;
			derta = unit_side_length_ / 2;
			//����������ĵ�����;
			pt.x = (i + 0.5)*unit_side_length_ - extract_radius_;
			pt.y = (j + 0.5)*unit_side_length_ - extract_radius_;
			//���������ڵĵ�;
			search_indices.swap(vector<int>());
			distances.swap(vector<float>());
			tree.radiusSearch(pt, 1.5*unit_side_length_, search_indices, distances);
		
			if (distances.empty() != true)
			{
				//�����ڵĵ��Ȩ������������еĵ����,ȨֵΪ�����˹����exp(-(x-u)*(x-u)* / (2 * derta * derta));
				for (size_t n = 0; n < search_indices.size(); n++)
				{
					grid[i + j*voxel_side_num_].point_num += exp(-distances[n] / (2 * derta * derta));
					float depth;
					depth = rotated_cloud->points[search_indices[n]].z + extract_radius_;
					grid[i + j*voxel_side_num_].average_depth += depth*exp(-distances[n] / (2 * derta * derta));
				}
			}
		}
	}
	

	//�������е���XOZ��ͶӰ,��������ÿ�������еĵ����;
	pcl::PointCloud<pcl::PointXY>::Ptr cloud_xz(new  pcl::PointCloud<pcl::PointXY>);
	for (size_t i = 0; i < rotated_cloud->size(); i++)
	{
		pcl::PointXY pt;
		pt.x = rotated_cloud->points[i].x;
		pt.y = rotated_cloud->points[i].z;
		cloud_xz->points.push_back(pt);
	}
	tree.setInputCloud(cloud_xz);

	for (size_t i = 0; i < voxel_side_num_; i++)
	{
		for (size_t j = 0; j < voxel_side_num_; j++)
		{
			pcl::PointXY pt;
			float derta;
			derta = unit_side_length_ / 2;
			pt.x = (i + 0.5)*unit_side_length_ - extract_radius_;
			pt.y = (j + 0.5)*unit_side_length_ - extract_radius_;
			tree.radiusSearch(pt, 1.5*unit_side_length_, search_indices, distances);
			if (distances.empty() != true)
			{
				for (size_t n = 0; n < search_indices.size(); n++)
				{
					grid[i + j*voxel_side_num_ + voxel_side_num_*voxel_side_num_].point_num += exp(-distances[n] / (2 * derta * derta));
					float depth;
					depth = rotated_cloud->points[search_indices[n]].y + extract_radius_;
					grid[i + j*voxel_side_num_ + voxel_side_num_*voxel_side_num_].average_depth += depth*exp(-distances[n] / (2 * derta * derta));
				}
			}
		}
	}


	//�������е���YOZ��ͶӰ,��������ÿ�������еĵ����;
	pcl::PointCloud<pcl::PointXY>::Ptr cloud_yz(new  pcl::PointCloud<pcl::PointXY>);
	for (size_t i = 0; i < rotated_cloud->size(); i++)
	{
		pcl::PointXY pt;
		pt.x = rotated_cloud->points[i].y;
		pt.y = rotated_cloud->points[i].z;
		cloud_yz->points.push_back(pt);
	}
	tree.setInputCloud(cloud_yz);

	for (size_t i = 0; i < voxel_side_num_; i++)
	{
		for (size_t j = 0; j < voxel_side_num_; j++)
		{
			pcl::PointXY pt;
			float derta;
			derta = unit_side_length_ / 2;
			pt.x = (i + 0.5)*unit_side_length_ - extract_radius_;
			pt.y = (j + 0.5)*unit_side_length_ - extract_radius_;
			tree.radiusSearch(pt, 1.5*unit_side_length_, search_indices, distances);
			if (distances.empty() != true)
			{
				for (size_t n = 0; n < search_indices.size(); n++)
				{
					grid[i + j*voxel_side_num_ + 2 * voxel_side_num_*voxel_side_num_].point_num += exp(-distances[n] / (2 * derta * derta));
					float depth;
					depth = rotated_cloud->points[search_indices[n]].x + extract_radius_;
					grid[i + j*voxel_side_num_ + 2 * voxel_side_num_*voxel_side_num_].average_depth += depth*exp(-distances[n] / (2 * derta * derta));
				}
			}		
		}
	}


	/*δ���Ǳ�ԵЧӦ�����;*/
	/*for (size_t i = 0; i < rotated_cloud->size(); i++)
	{
		x = getVoxelNum(rotated_cloud->points[i].x);
		y = getVoxelNum(rotated_cloud->points[i].y);
		index = x + y*voxel_side_num_ ;
		grid[index].point_num++;
	}

	for (size_t i = 0; i < rotated_cloud->size(); i++)
	{
		x = getVoxelNum(rotated_cloud->points[i].x);
		z = getVoxelNum(rotated_cloud->points[i].z);
		index = x + z*voxel_side_num_ + voxel_side_num_*voxel_side_num_;
		grid[index].point_num++;
	}

	for (size_t i = 0; i < rotated_cloud->size(); i++)
	{
		y = getVoxelNum(rotated_cloud->points[i].y);
		z = getVoxelNum(rotated_cloud->points[i].z);
		index = y + z*voxel_side_num_ + 2 * voxel_side_num_*voxel_side_num_;
		grid[index].point_num++;
	}*/

	//�������и���,����ÿ�����ӵĹ�һ��Ȩֵ(Ng/Vg)/(Nn/Vn);
	//NgΪÿ�������еĵ����,VgΪÿ�����ӵ����;NnΪ�ֲ������ڵ����е�,VnΪ�ֲ���������;
	float grid_density, grid_area, neighbourhood_density, neighbourhood_area;
	//����ֲ������ڵĵ��ܶ�;
	neighbourhood_area = M_PI*extract_radius_*extract_radius_;
	neighbourhood_density = rotated_cloud->size() / neighbourhood_area;

	for (size_t i = 0; i < grid.size(); i++)
	{
		//����ÿ�����ӵ��ƽ�����;
		if (grid[i].point_num == 0.0)
		{
			grid[i].average_depth = 0.0f;
		}
		else
		{
			grid[i].average_depth /= grid[i].point_num;
		}
		
		//����ÿ�������ĵ��ܶ�;
		grid_area = unit_side_length_*unit_side_length_;
		grid_density = grid[i].point_num / grid_area;
		//����ÿ�������Ĺ�һ��Ȩֵ;
		if (neighbourhood_density != 0.0f)
		{
			grid[i].normalized_point_weight = grid_density / neighbourhood_density;
		}
		else
		{
			grid[i].normalized_point_weight = 0.0f;
		}
	}
}

//��2άƽ����������������;
void StereoBinaryFeatureExtractor::randomSamplePointPairs()
{
	//ʹ��128�����������
	for (int i = 0; i < voxel_side_num_*voxel_side_num_ * 2; i++)
	{
		int pair1 = rand() % voxel_side_num_*voxel_side_num_;
		int pair2 = rand() % voxel_side_num_*voxel_side_num_;
		while (pair1 == pair2 || contain2DPair(pair1, pair2))
		{
			pair1 = rand() % voxel_side_num_*voxel_side_num_;
			pair2 = rand() % voxel_side_num_*voxel_side_num_;
		}
		grid_index_pairs_2d_.push_back(pair<int, int>(pair1, pair2));
	}

}



//�������������������ͶӰ���ͶӰ�����������Ƚϵ�����;
StereoBinaryFeature  StereoBinaryFeatureExtractor::computeFeatureProjectedGridAndCompareFeature(const vector<GridVoxel> & grid)
{
	float normalized_point_weightT(0.1);
	StereoBinaryFeature result_feature(gridFeatureDimension_ + compairFeatureDimension_);

	//����ÿ�������Ƿ�Ϊ��,�õ�����������;
	for (int i = 0; i<grid.size(); i++)
	{
		if (grid[i].normalized_point_weight > normalized_point_weightT)
		{
			int bit_num = i % 8;
			int byte_num = i / 8;
			char test_num = 1 << bit_num;
			result_feature.feature_[byte_num] |= test_num;
		}
	}

	/*����ƽ��������ܶ������Ƚϵ�����;*/
	//�������еĵ��,��������֮����ܶȱ仯�ľ�ֵ�ͱ�׼��;
	double average(0.0), variance(0.0), standardDeviation(0.0), x(0.0);
	for (int i = 0; i < grid_index_pairs_2d_.size(); i++)
	{
		x = grid[grid_index_pairs_2d_[i].first].normalized_point_weight - grid[grid_index_pairs_2d_[i].second].normalized_point_weight;
		average += x;
	}
	average /= grid_index_pairs_2d_.size();

	for (int i = 0; i < grid_index_pairs_2d_.size(); i++)
	{
		x = grid[grid_index_pairs_2d_[i].first].normalized_point_weight - grid[grid_index_pairs_2d_[i].second].normalized_point_weight;
		variance += (x - average)*(x - average);
	}
	variance /= grid_index_pairs_2d_.size();
	standardDeviation = sqrt(variance);

	for (int i = 0; i<grid_index_pairs_2d_.size(); i++)
	{
		//����Ƚϵ���������Ϊ��,��ȡֵΪ0;���һ������Ϊ��,��һ��Ϊ�ǿ�,��ȡֵΪ1;
		if ((grid[grid_index_pairs_2d_[i].first].normalized_point_weight > normalized_point_weightT
			&&grid[grid_index_pairs_2d_[i].second].normalized_point_weight < normalized_point_weightT)
			|| (grid[grid_index_pairs_2d_[i].first].normalized_point_weight < normalized_point_weightT
			&&grid[grid_index_pairs_2d_[i].second].normalized_point_weight > normalized_point_weightT))
		{
			x = grid[grid_index_pairs_2d_[i].first].normalized_point_weight - grid[grid_index_pairs_2d_[i].second].normalized_point_weight;
			int k = i + (int)grid.size();
			int bit_num = k % 8;
			int byte_num = k / 8;
			char test_num = 1 << bit_num;
			result_feature.feature_[byte_num] |= test_num;
		}

		// ����������Ӷ�Ϊ�ǿ�, ����бȽ�;
		if (grid[grid_index_pairs_2d_[i].first].normalized_point_weight > normalized_point_weightT
			&&grid[grid_index_pairs_2d_[i].second].normalized_point_weight > normalized_point_weightT)
		{
			x = grid[grid_index_pairs_2d_[i].first].normalized_point_weight - grid[grid_index_pairs_2d_[i].second].normalized_point_weight;
			if (abs(x - average) > standardDeviation)
			{
				int k = i + (int)grid.size();
				int bit_num = k % 8;
				int byte_num = k / 8;
				char test_num = 1 << bit_num;
				result_feature.feature_[byte_num] |= test_num;
			}
		}

	}

	return result_feature;
}


//�������������������ͶӰ���ͶӰ�����������Ƚϵ�����(ÿ��ͶӰ��ֱ�Ƚ�);
StereoBinaryFeature  StereoBinaryFeatureExtractor::computeFeatureProjectedGridAndCompareFeature2D(const vector<GridVoxel> & grid)
{
	float normalized_point_weightT(0.1);
	StereoBinaryFeature result_feature(gridFeatureDimension_ + compairFeatureDimension_);
	int featureDimenshions(0);

	//����ÿ�������Ƿ�Ϊ��,�õ�����������;
	for (int i = 0; i<grid.size(); i++)
	{
		if (grid[i].normalized_point_weight > normalized_point_weightT)
		{
			int bit_num = i % 8;
			int byte_num = i / 8;
			char test_num = 1 << bit_num;
			result_feature.feature_[byte_num] |= test_num;
		}
		featureDimenshions++;
	}
	
	

	//XOY,XOZ,YOZ ����������,�������еĵ��,��������֮����ȱ仯�ľ�ֵ�ͱ�׼��;
	double average_depth(0.0), variance_depth(0.0);
	double standardDeviation_depth(0.0), depth(0.0);
	double average_density(0.0), variance_density(0.0);
	double standardDeviation_density(0.0), density(0.0);
	int offset(0);

	for (int nn = 0; nn < 3; nn++)
	{
		average_depth = 0.0; variance_depth = 0.0; standardDeviation_depth = 0.0; depth = 0.0;
		average_density = 0.0; variance_density = 0.0; standardDeviation_density = 0.0; density = 0.0;


		for (int i = 0; i < grid_index_pairs_2d_.size(); i++)
		{
			depth = grid[grid_index_pairs_2d_[i].first + offset].average_depth - grid[grid_index_pairs_2d_[i].second + offset].average_depth;
			average_depth += depth;

			density = grid[grid_index_pairs_2d_[i].first + offset].normalized_point_weight - grid[grid_index_pairs_2d_[i].second + offset].normalized_point_weight;
			average_density += density;

		}
		average_depth /= grid_index_pairs_2d_.size();
		average_density /= grid_index_pairs_2d_.size();


		for (int i = 0; i < grid_index_pairs_2d_.size(); i++)
		{
			depth = grid[grid_index_pairs_2d_[i].first + offset].average_depth - grid[grid_index_pairs_2d_[i].second + offset].average_depth;
			variance_depth += (depth - average_depth)*(depth - average_depth);

			density = grid[grid_index_pairs_2d_[i].first + offset].normalized_point_weight - grid[grid_index_pairs_2d_[i].second + offset].normalized_point_weight;
			variance_density += (density - average_density)*(density - average_density);

		}
		variance_depth /= grid_index_pairs_2d_.size();
		standardDeviation_depth = sqrt(variance_depth);

		variance_density /= grid_index_pairs_2d_.size();
		standardDeviation_density = sqrt(variance_density);



		for (int i = 0; i<grid_index_pairs_2d_.size(); i++)
		{
			// �����������ӵ�ƽ����Ȳ�;
			depth = grid[grid_index_pairs_2d_[i].first + offset].average_depth - grid[grid_index_pairs_2d_[i].second + offset].average_depth;
			if (abs(depth - average_depth) > standardDeviation_depth)
			{
				int k = featureDimenshions;
				int bit_num = k % 8;
				int byte_num = k / 8;
				char test_num = 1 << bit_num;
				result_feature.feature_[byte_num] |= test_num;
				//cout << 1 << endl;
			}
			featureDimenshions++;

			// �����������ӵ�ƽ���ܶȲ�;
			if (grid[grid_index_pairs_2d_[i].first].normalized_point_weight < normalized_point_weightT
				&&grid[grid_index_pairs_2d_[i].second].normalized_point_weight < normalized_point_weightT)
			{
				//����������Ӷ�Ϊ��,���λ�õ�����Ϊ0;
			}	
			else
			{
				density = grid[grid_index_pairs_2d_[i].first + offset].normalized_point_weight - grid[grid_index_pairs_2d_[i].second + offset].normalized_point_weight;
				if (abs(density - average_density) > standardDeviation_density)
				{
					int k = featureDimenshions;
					int bit_num = k % 8;
					int byte_num = k / 8;
					char test_num = 1 << bit_num;
					result_feature.feature_[byte_num] |= test_num;
				}
			}
			featureDimenshions++;
		}

		offset += (voxel_side_num_*voxel_side_num_);
	}
	return result_feature;
}


StereoBinaryFeature  StereoBinaryFeatureExtractor::computeFeatureProjectedGrid(const vector<GridVoxel> & grid)		//������� �������������
{
	float normalized_point_weightT(0.1);
	StereoBinaryFeature result_feature(gridFeatureDimension_);

	//����ÿ�������Ƿ�Ϊ��,�õ�����������;
	for (int i = 0; i<grid.size(); i++)
	{
		if (grid[i].normalized_point_weight > normalized_point_weightT)
		{
			int bit_num = i % 8;
			int byte_num = i / 8;
			char test_num = 1 << bit_num;
			result_feature.feature_[byte_num] |= test_num;
		}
	}
	return result_feature;
}

/*�����ɵ���ά������ֵ����Ϊ��������*/
StereoBinaryFeature StereoBinaryFeatureExtractor::computeFeatureBinarizeGrid(const vector<GridVoxel> & grid)		//������� �������������
{
	//�ж���bit �������ٴ�
	StereoBinaryFeature result_feature(grid.size());

	for(int i=0;i<grid.size();i++)
	{
		if(grid[i].normalized_point_weight > 0.2)
		{
			int bit_num = i%8;
			int byte_num = i/8;
			char test_num = 1<<bit_num;
			result_feature.feature_[byte_num]|=test_num;
		}
	}
	return result_feature;
}

bool StereoBinaryFeatureExtractor::extractBinaryFeatureOfKeypoint(const pcXYZIPtr & input_cloud,size_t ptIndex, const std::vector<int> &searchIndexes,
										                          vector<StereoBinaryFeature> & features)
{
	StereoBinaryFeature feature;

	if (searchIndexes.empty())
	{
		cout << "����Χ��δ�ҵ��㡣" << endl;
		features.push_back(feature);
		features.push_back(feature);
		return false;
	}

	//�洢�����ṹ
	vector<GridVoxel> grid(gridFeatureDimension_);

	CoordinateSystem localSystem1;
	pcl::PointCloud<pcl::PointXYZI>::Ptr result_cloud(new pcl::PointCloud<pcl::PointXYZI>);
		

	computeLocalCoordinateSystem(input_cloud, ptIndex, searchIndexes, localSystem1);
	transformPointCloudToLocalSystem(input_cloud, ptIndex, searchIndexes, localSystem1, result_cloud);

	//�������� 
	constructCubicGrid(result_cloud, grid);

	//�Ӹ�����������
	feature = computeFeatureProjectedGridAndCompareFeature2D(grid);
	feature.localSystem_.xAxis = localSystem1.xAxis;//���ֲ������2D������ֵ;
	feature.localSystem_.yAxis = localSystem1.yAxis;//���ֲ������2D������ֵ;
	feature.localSystem_.zAxis = localSystem1.zAxis;//���ֲ������2D������ֵ;
	feature.localSystem_.origin = localSystem1.origin;//���ֲ������2D������ֵ;
	features.push_back(feature);

	//�����������grid��result_cloud ������һ�μ���
	grid.swap(vector<GridVoxel>(gridFeatureDimension_));
	result_cloud->points.clear();

	/*Z�᲻��,X���Y�ᷴ��;*/
	CoordinateSystem localSystem2;
	localSystem2.xAxis = -localSystem1.xAxis;
	localSystem2.yAxis = -localSystem1.yAxis;
	localSystem2.zAxis = localSystem1.zAxis;
	localSystem2.origin = localSystem1.origin;
	transformPointCloudToLocalSystem(input_cloud, ptIndex, searchIndexes, localSystem2, result_cloud);
	//�������� 
	constructCubicGrid(result_cloud, grid);

	//�Ӹ�����������
	feature = computeFeatureProjectedGridAndCompareFeature2D(grid);
	feature.localSystem_.xAxis = localSystem2.xAxis;//���ֲ������2D������ֵ;
	feature.localSystem_.yAxis = localSystem2.yAxis;//���ֲ������2D������ֵ;
	feature.localSystem_.zAxis = localSystem2.zAxis;//���ֲ������2D������ֵ;
	feature.localSystem_.origin = localSystem2.origin;//���ֲ������2D������ֵ;
	features.push_back(feature);

	//�����������grid��result_cloud ������һ�μ���
	grid.swap(vector<GridVoxel>(gridFeatureDimension_));
	result_cloud->points.clear();
	pcXYZI().swap(*result_cloud);

	return true;
}

/*��ȡ��������,��ȡʧ���򷵻ص�vectorΪ��*/
void StereoBinaryFeatureExtractor::extractBinaryFeatures(const pcXYZIPtr & input_cloud,const PointIndicesPtr &indices,doubleVectorSBF & bscFeatures)
														 
{
	size_t feature_num;	//��������������
	if (indices->indices.size() > 0)
	{
		feature_num = indices->indices.size();
	}
	else
	{
		cout << "�����������Ϊ��\n";
		return;
	}

	//���г�ʼ��
	vector<StereoBinaryFeature> features_coor1(feature_num);
	vector<StereoBinaryFeature> features_coor2(feature_num);

	//KD������;
	pcl::KdTreeFLANN<pcl::PointXYZI> tree;
	tree.setInputCloud(input_cloud);


	concurrency::parallel_for(size_t(0), feature_num, [&](size_t i)
	{
		vector<StereoBinaryFeature> features;
		features.swap(vector<StereoBinaryFeature>());

		//�����������ñ���
		vector<int> searchIndexes;
		vector<float> distances;
		searchIndexes.swap(vector<int>());
		distances.swap(vector<float>());
		size_t ptIndex;
		ptIndex = indices->indices[i];
		tree.radiusSearch(ptIndex, sqrt(3.0)*extract_radius_, searchIndexes, distances);

		extractBinaryFeatureOfKeypoint(input_cloud, ptIndex, searchIndexes, features);
		features_coor1[i] = features[0];
		features_coor2[i] = features[1];

		features_coor1[i].keypointIndex_ = i;
		features_coor2[i].keypointIndex_ = i;	
	});

	bscFeatures.push_back(features_coor1);
	bscFeatures.push_back(features_coor2);

	features_coor1.swap(vector<StereoBinaryFeature>());
	features_coor2.swap(vector<StereoBinaryFeature>());
}		