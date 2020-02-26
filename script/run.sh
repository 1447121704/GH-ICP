#data path
#target_point_cloud_path=/media/edward/BackupPlus/Data/ETH_registration_TLS/arch/PointCloud_pcd/s1.pcd;
#source_point_cloud_path=/media/edward/BackupPlus/Data/ETH_registration_TLS/arch/PointCloud_pcd/s2.pcd;
#output_point_cloud_path=/media/edward/BackupPlus/Data/ETH_registration_TLS/arch/PointCloud_pcd/reg_s2.pcd;
target_point_cloud_path=/home/edward/testdata/building1.pcd
source_point_cloud_path=/home/edward/testdata/building2.pcd
output_point_cloud_path=/home/edward/testdata/building2_reg.pcd

#parameters
using_feature=B;              # Feature selection [ B: BSC, F: FPFH, R: RoPS, N: register without feature ]
corres_estimation_method=K;   # Correspondence estimation by [ K: Bipartite graph min weight match using KM, N: Nearest Neighbor, R: Reciprocal NN ]

downsample_resolution=0.2;    # Raw data downsampling voxel size, just keep one point in the voxel  
neighborhood_radius=0.6;      # Curvature estimation / feature encoding radius
curvature_non_max_radius=1.8; # Keypoint extraction based on curvature: non max suppression radius 
weight_adjustment_ratio=1.1;  # Weight would be adjusted if the IoU between expected value and calculated value is beyond this value
weight_adjustment_step=0.1;   # Weight adjustment for one iteration

appro_overlap_ratio=0.7;      # Estimated approximate overlapping ratio of two point cloud 

#run
./bin/ghicp ${target_point_cloud_path} ${source_point_cloud_path} ${output_point_cloud_path} \
${using_feature} ${corres_estimation_method} \
${downsample_resolution} ${neighborhood_radius} ${curvature_non_max_radius} \
${weight_adjustment_ratio} ${weight_adjustment_step} ${appro_overlap_ratio}

