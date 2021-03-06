// We will subscribe to colored UFOMaps in this tutorial
#include <ufo/map/occupancy_map.h>
#include <ufo/map/occupancy_map_color.h>
// UFOMap ROS msg
#include <ufomap_msgs/UFOMapStamped.h>
// To convert between UFO and ROS
#include <ufomap_msgs/conversions.h>
#include <ufomap_ros/conversions.h>
#include <ros/ros.h>
#include <list>
#include <iostream>
#include <chrono>
#include <visualization_msgs/Marker.h>
#include "nav_msgs/Odometry.h"
#include <math.h>

using namespace std::chrono;
using namespace std;
using namespace std::this_thread;

struct node{
   public:
      ufo::math::Vector3* point;
      node* myParent;
      double distanceToParent;
      std::list<struct node*> myChilds{};
      std::list<struct node*> myPath{};
      std::list<ufo::math::Vector3> myHits{};
      double distanceToGoal;
      node(float x, float y, float z){
         point = new ufo::math::Vector3(x, y, z);
      }
      ufo::math::Vector3* getNodePointer(){
         return point;
      }
      void addChild(node* newChild){
        myChilds.push_back(newChild);
      }
      void addParent(node* newParent){
        myParent = newParent;
      }
      void changeDistanceToGoal(double newDistance){
        distanceToGoal = newDistance;
      }
      void changeDistanceToParent(){
        if(myParent != nullptr){
          distanceToParent = sqrt(pow(point->x() - myParent->point->x(), 2) + pow(point->y() - myParent->point->y(), 2) + pow(point->z() - myParent->point->z(), 2));
        }
        else{
          distanceToParent = 0;
        }
      }
      double sumDistance(){
        changeDistanceToParent();
        double myDistance = 0;
        if(myParent != nullptr){
          myDistance = myParent->sumDistance();
          changeDistanceToParent();
        }
        if(myParent == nullptr){
          return 0;
        }
        return (myDistance + distanceToParent);
      }
      
      void getPath(std::list<struct node*>* givenPath){
        if(myParent != nullptr){
          myParent->getPath(givenPath);
          givenPath->push_back(myParent);
        }
        if(myParent == nullptr){
          return;
        }
        return;
      }
      
      int findInformationGain(float SCALER_AABB, ufo::map::OccupancyMapColor const& map){
        if(myHits.empty()){
          //Setting up targets, X / Y targets in either positive (P) or negative (N) direction
          ufo::math::Vector3 targetXP(point->x() + 10.0, point->y(), point->z());
          ufo::math::Vector3 targetXN(point->x() - 10.0, point->y(), point->z());
          ufo::math::Vector3 targetYP(point->x(), point->y() + 10.0, point->z());
          ufo::math::Vector3 targetYN(point->x(), point->y() - 10.0, point->z());
          ufo::math::Vector3 upwards(point->x(), point->y(), point->z() + 10.0);
        
          //Angles
          double vertical_angle = 30;
          double horizontal_angle = 45;
        
          //Distances
          double near_distance = 0.1;
          double far_distance = 10;
        
          //Frustums
          ufo::geometry::Frustum frustXP(*point, targetXP, upwards, vertical_angle, horizontal_angle, near_distance, far_distance);
          ufo::geometry::Frustum frustXN(*point, targetXN, upwards, vertical_angle, horizontal_angle, near_distance, far_distance);
          ufo::geometry::Frustum frustYP(*point, targetYP, upwards, vertical_angle, horizontal_angle, near_distance, far_distance);
          ufo::geometry::Frustum frustYN(*point, targetYN, upwards, vertical_angle, horizontal_angle, near_distance, far_distance);
        
          for (auto it = map.beginLeaves(frustXP, false, false, true, false, 0), it_end = map.endLeaves(); it != it_end; ++it){
            ufo::math::Vector3 end_point(it.getX(), it.getY(), it.getZ());
            ufo::geometry::LineSegment myLine(*point, end_point);
            if(!isInCollision(map, myLine, true, false, false, 0)){
              myHits.push_back(end_point);
            }
          }
          for (auto it = map.beginLeaves(frustXN, false, false, true, false, 0), it_end = map.endLeaves(); it != it_end; ++it){
            ufo::math::Vector3 end_point(it.getX(), it.getY(), it.getZ());
            ufo::geometry::LineSegment myLine(*point, end_point);
            if(!isInCollision(map, myLine, true, false, false, 0)){
              myHits.push_back(end_point);
            }
          }
          for (auto it = map.beginLeaves(frustYP, false, false, true, false, 0), it_end = map.endLeaves(); it != it_end; ++it){
            ufo::math::Vector3 end_point(it.getX(), it.getY(), it.getZ());
            ufo::geometry::LineSegment myLine(*point, end_point);
            if(!isInCollision(map, myLine, true, false, false, 0)){
              myHits.push_back(end_point);
            }
          }
          for (auto it = map.beginLeaves(frustYN, false, false, true, false, 0), it_end = map.endLeaves(); it != it_end; ++it){
            ufo::math::Vector3 end_point(it.getX(), it.getY(), it.getZ());
            ufo::geometry::LineSegment myLine(*point, end_point);
            if(!isInCollision(map, myLine, true, false, false, 0)){
              myHits.push_back(end_point);
            }
          }
          if(myParent != nullptr){
            myParent->findInformationGain(SCALER_AABB, map);
          }
        }
        std::list<ufo::math::Vector3> myTotalHits{};
        addHits(&myTotalHits);
        int hits = myTotalHits.size();
        return hits;
      }
      
      void addHits(std::list<ufo::math::Vector3>* hitList){
        bool add = true;
        for(auto it = myHits.begin(), it_end = myHits.end(); it != it_end; ++it){
          for(auto it2 = hitList->begin(), it_end2 = hitList->end(); it2 != it_end2; ++it2){
            if(it->x() == it2->x() and it->y() == it2->y() and it->z() == it2->z()){
              add = false;
              break;
            }
          }
          if(add){
            hitList->push_back(*it);
          }
          add = true;
        }
        if(myParent != nullptr){
          myParent->addHits(hitList);
        }
      };
      
      bool findPathImprovement(struct node* targetNode, ufo::map::OccupancyMapColor const& map){
        bool improvementFound;
        if(myParent != nullptr){
          improvementFound = myParent->findPathImprovement(targetNode, map);
        }else{
          ufo::geometry::LineSegment myLine(*(targetNode->point), *point);
          if(!isInCollision(map, myLine, true, false, true, 0)){
            targetNode->addParent(this);
            return true;
          }else{
            return false;
          }
        }
        if(!improvementFound){
          ufo::geometry::LineSegment myLine(*(targetNode->point), *point);
          if(!isInCollision(map, myLine, true, false, true, 0)){
            targetNode->addParent(this);
            improvementFound = findPathImprovement(this, map);
            return improvementFound;
          }else{
            return false;
          }
        }else{
          return improvementFound;
        }
      }
      
      bool isInCollision(ufo::map::OccupancyMapColor const& map, 
                   ufo::geometry::BoundingVar const& bounding_volume, 
                   bool occupied_space = true, bool free_space = false,
                   bool unknown_space = false, ufo::map::DepthType min_depth = 0){
        // Iterate through all leaf nodes that intersects the bounding volume
        for (auto it = map.beginLeaves(bounding_volume, occupied_space, 
                                 free_space, unknown_space, false, min_depth), 
        it_end = map.endLeaves(); it != it_end; ++it) {
          // Is in collision since a leaf node intersects the bounding volume.
          return true;
        }
        // No leaf node intersects the bounding volume.
        return false;
      }
};

// Variables of interest
int NUMBER_OF_NODES = 3000;
int NUMBER_OF_GOALS = 40;
int NUMBER_OF_ITTERATIONS = 3000;
bool RUN_BY_NODES = true;
double SENSOR_RANGE = 2;
double SCALER_INFORMATION_GAIN = 1;
int itterations;
int STEP_LENGTH = 1;
float SCALER_AABB = 20;
float SCALER_X = SCALER_AABB;
float SCALER_Y = SCALER_AABB;
float SCALER_Z = SCALER_AABB;
double radius = 0.5;
bool map_received = false;
bool RRT_created = false;
bool GOALS_generated = false;
bool position_received = false;
bool fetched_path = false;
float position_x = 0;
float position_y = 0;
float position_z = 0;
double arbitraryDistance = 0.5;
struct node* goalNode;
float lowest_x;
float lowest_y;
float lowest_z;
float highest_x;
float highest_y;
float highest_z;


// Create a colored UFOMap
ufo::map::OccupancyMapColor myMap(0.7);
std::list<struct node> RRT_TREE{};
std::list<struct node*> PATH{};
std::list<struct node> myGoals{};
std::list<ufo::math::Vector3> hits{};

void tuneGeneration(ufo::map::OccupancyMapColor const& map, bool occupied_space, bool free_space, bool unknown_space, ufo::map::DepthType min_depth = 4){
  highest_x = std::numeric_limits<float>::min();
  highest_y = std::numeric_limits<float>::min();
  highest_z = std::numeric_limits<float>::min();
  lowest_x = std::numeric_limits<float>::max();
  lowest_y = std::numeric_limits<float>::max();
  lowest_z = std::numeric_limits<float>::max();
  ufo::math::Vector3 minPoint(position_x - 1 * SCALER_AABB, position_y - 1 * SCALER_AABB, position_z - 1 * SCALER_AABB);
  ufo::math::Vector3 maxPoint(position_x + 1 * SCALER_AABB, position_y + 1 * SCALER_AABB, position_z + 1 * SCALER_AABB);
  ufo::geometry::AABB aabb(minPoint, maxPoint);
  for (auto it = map.beginLeaves(aabb, occupied_space, free_space, unknown_space, false, min_depth), it_end = map.endLeaves(); it != it_end; ++it) {
    if(it.getX() > highest_x){
      highest_x = it.getX();
    }if(it.getX() < lowest_x){
      lowest_x = it.getX();
    }
    if(it.getY() > highest_y){
      highest_y = it.getY();
    }if(it.getY() < lowest_y){
      lowest_y = it.getY();
    }
    if(it.getZ() > highest_z){
      highest_z = it.getZ();
    }if(it.getZ() < lowest_z){
      lowest_z = it.getZ();
    }
  }
  SCALER_X = highest_x - lowest_x;
  SCALER_Y = highest_y - lowest_y;
  SCALER_Z = highest_z - lowest_z;
}

bool isInCollision(ufo::map::OccupancyMapColor const& map, 
                   ufo::geometry::BoundingVar const& bounding_volume, 
                   bool occupied_space = true, bool free_space = false,
                   bool unknown_space = false, ufo::map::DepthType min_depth = 0)
{
  // Iterate through all leaf nodes that intersects the bounding volume
  for (auto it = map.beginLeaves(bounding_volume, occupied_space, 
                                 free_space, unknown_space, false, min_depth), 
        it_end = map.endLeaves(); it != it_end; ++it) {
    // Is in collision since a leaf node intersects the bounding volume.
    return true;
  }
  // No leaf node intersects the bounding volume.
  return false;
}

void findShortestPath(){
  for(std::list<node>::iterator it_goals = myGoals.begin(); it_goals != myGoals.end(); it_goals++){
    struct node* chosenNode;
    double distance = std::numeric_limits<double>::max();
    for(std::list<node>::iterator it_RRT = RRT_TREE.begin(); it_RRT != RRT_TREE.end(); it_RRT++){
      double distanceNodeToGoal = sqrt(pow(it_RRT->point->x() - it_goals->point->x(), 2) + pow(it_RRT->point->y() - it_goals->point->y(), 2) + pow(it_RRT->point->z() - it_goals->point->z(), 2));
      double distanceToNode = it_RRT->sumDistance();
      double totalDistance = distanceNodeToGoal + distanceToNode;
      if(totalDistance < distance){
        ufo::geometry::LineSegment myLine(*(it_goals->point), *(it_RRT->point));
        if(!isInCollision(myMap, myLine, true, false, true, 0)){
          distance = totalDistance;
          chosenNode = &*it_RRT;
        }
      }
    }
    it_goals->addParent(chosenNode);
    chosenNode->addChild(&*it_goals);
  }
}

void generateGoals(ufo::map::OccupancyMapColor const& map){
  // Generate goals, check for occupancy status
  ufo::math::Vector3 goal;
  srand(time(0));
  while((myGoals.size() < NUMBER_OF_GOALS) and (itterations < 10000)){
    float x = lowest_x + abs(1024 * rand () / (RAND_MAX + 1.0)) * SCALER_X;
    float y = lowest_y + abs(1024 * rand () / (RAND_MAX + 1.0)) * SCALER_Y;
    float z = lowest_z + abs(1024 * rand () / (RAND_MAX + 1.0)) * SCALER_Z;
    ufo::math::Vector3 goal(x, y, z);
    ufo::geometry::Sphere goal_sphere(goal, radius);
    if(!isInCollision(myMap, goal_sphere, true, false, true, 0) and isInCollision(myMap, goal_sphere, false, true, false, 0)){
      ufo::math::Vector3 min_point(x - SENSOR_RANGE, y - SENSOR_RANGE, z - SENSOR_RANGE);
      ufo::math::Vector3 max_point(x + SENSOR_RANGE, y + SENSOR_RANGE, z + SENSOR_RANGE);
      ufo::math::Vector3 position(position_x, position_y, position_z);
      ufo::geometry::AABB aabb(min_point, max_point);
      for (auto it = map.beginLeaves(aabb, false, false, true, false, 4), it_end = map.endLeaves(); it != it_end; ++it) {
        if (it.isUnknown()) {
          ufo::math::Vector3 unknownNode(it.getX(), it.getY(), it.getZ());
          ufo::geometry::LineSegment myLine(goal, unknownNode);
          if(!isInCollision(map, myLine, true, false, false, 0)){
            node newGoal(x, y, z);
            myGoals.push_back(newGoal);
            break;
          }
        }
      break;
      }
    };
      itterations++;
  };
  if(myGoals.size() == NUMBER_OF_GOALS){
    std::cout << "Goals generated successfully\n" << std::endl;
    GOALS_generated = true;
  }else if(myGoals.size() == 0){
    std::cout << "No goals found, trying again soon" << std::endl;
    sleep_for(microseconds(100000));
  }else{
    std::cout << "Only " << myGoals.size() << " goals found" << std::endl;
    GOALS_generated = true;
  }
};

void setPath(){
  double totalCost = std::numeric_limits<float>::max();
  double newCost = std::numeric_limits<float>::max();
  for(std::list<node>::iterator it_goal = myGoals.begin(); it_goal != myGoals.end(); it_goal++){
    it_goal->findPathImprovement(&*it_goal, myMap);
    newCost = it_goal->sumDistance() - SCALER_INFORMATION_GAIN * (it_goal->findInformationGain(SCALER_AABB, myMap));
    std::cout << "My hits = " << it_goal->findInformationGain(SCALER_AABB, myMap) << std::endl;
    if(newCost < totalCost){
      totalCost = newCost;
      goalNode = &*it_goal;
    }
  }
}

void generateRRT(){
  RRT_TREE.clear();
  std::cout << "Building RRT-tree" << std::endl;
  float step_length = 1;
  float sensor_range = 2;
  node origin(position_x, position_y, position_z);
  origin.addParent(nullptr);
  RRT_TREE.push_back(origin);
  srand(time(0));
  while(((RRT_TREE.size() <= NUMBER_OF_NODES and RUN_BY_NODES) or (itterations <= NUMBER_OF_ITTERATIONS and !RUN_BY_NODES)) and itterations < 100000){
    // Generate a random point
    float x = lowest_x + abs(1024 * rand () / (RAND_MAX + 1.0)) * SCALER_X;
    float y = lowest_y + abs(1024 * rand () / (RAND_MAX + 1.0)) * SCALER_Y;
    float z = lowest_z + abs(1024 * rand () / (RAND_MAX + 1.0)) * SCALER_Z;
    ufo::math::Vector3 random_point(x, y, z);
    // If the point is not occupied, continue
    if(!isInCollision(myMap, random_point, true, false, true, 0) and isInCollision(myMap, random_point, false, true, false, 0)){
      // TO DO: check for unknown within sensor_range
      //Find closest node in the RRT-TREE, set up the relation and add to tree
      float distance = std::numeric_limits<float>::max();
      node* parent;
      std::list<node>::iterator it_node;
      for(it_node = RRT_TREE.begin(); it_node != RRT_TREE.end(); it_node++){
        ufo::math::Vector3 direction = random_point - *(it_node->point);
        double new_distance = abs(direction.norm());
        if(new_distance < distance){
          distance = new_distance;
          parent = &*it_node;
        }
      };
      ufo::math::Vector3 start_point(parent->point->x(), parent->point->y(), parent->point->z());
      ufo::geometry::LineSegment myLine(random_point, start_point);
      if(!isInCollision(myMap, myLine, true, false, true, 0)) {
        node new_node(x, y, z);
        new_node.addParent(parent);
        parent->addChild(&new_node);
        RRT_TREE.push_back(new_node);
      }
    };
    // find closest node in the RRT-TREE
    itterations++;
  };
  std::cout << "RRT-tree built successfully" << std::endl;
  
  if(RUN_BY_NODES){
    std::cout << "Verifying tree" << std::endl;
    int total_childs = 0;
    int total_parents = 0;
    std::list<node>::iterator it_comeon;
    for(it_comeon = RRT_TREE.begin(); it_comeon != RRT_TREE.end(); it_comeon++){
      total_childs = total_childs + it_comeon->myChilds.size();
      if(it_comeon->myParent != nullptr){
        total_parents = total_parents + 1;
      }
    };
    if(total_childs == NUMBER_OF_NODES){
      std::cout << "All children accounter for" << std::endl;
    }else{
      std::cout << "Expected " << NUMBER_OF_NODES << " children, but " << total_childs << " was found." << std::endl;
    };
    if(total_parents == NUMBER_OF_NODES){
      std::cout << "All parents accounter for" << std::endl;
    }else{
      std::cout << "Expected " << NUMBER_OF_NODES << " parents, but " << total_parents << " was found." << std::endl;
    };
  }else{
    std::cout << "Running by itterations, so the amount of nodes are unknown and hence can't be verified" << std::endl;
  }
};

void mapCallback(ufomap_msgs::UFOMapStamped::ConstPtr const& msg)
{
  // Convert ROS message to UFOMap
  if (ufomap_msgs::msgToUfo(msg->map, myMap)) {
    // Conversion was successful
    //std::cout << "Conversion sucessful" << std::endl;
    map_received = true;
  } else {
    std::cout << "Conversion failed" << std::endl;
  }
}

void odomCallback(const nav_msgs::Odometry::ConstPtr& msg){
  position_received = true;
  position_x = msg->pose.pose.position.x;
  position_y = msg->pose.pose.position.y;
  position_z = msg->pose.pose.position.z;
}

int main(int argc, char *argv[])
{ 
  ros::init(argc, argv, "RRT_TREE");
  ros::NodeHandle nh;
  ros::Publisher points_pub = nh.advertise<visualization_msgs::Marker>("RRT_NODES", 1);
  ros::Publisher path_pub = nh.advertise<visualization_msgs::Marker>("RRT_PATH", 1);
  ros::Publisher goal_pub = nh.advertise<visualization_msgs::Marker>("RRT_GOALS", 1);
  ros::Publisher map_pub = nh.advertise<ufomap_msgs::UFOMapStamped>("goe_map", 11);
  ros::Subscriber map_sub = nh.subscribe("ufomap_mapping_server_node/map_depth_4", 1, mapCallback);
  ros::Subscriber sub = nh.subscribe("odometry/imu", 1, odomCallback);
  ros::Publisher hits_pub = nh.advertise<visualization_msgs::Marker>("HITS", 1);
  ros::Rate rate(10);
  
  /*auto start = high_resolution_clock::now();
  createRRT();
  auto stop = high_resolution_clock::now();
  */
  
  //auto duration = duration_cast<microseconds>(stop - start);
  //cout << "\nExecution time: " << duration.count() << " micro seconds for " << NUMBER_OF_NODES << " itterations." << endl;
  while(ros::ok()){
    if(map_received and not GOALS_generated){
      itterations = 0;
      tuneGeneration(myMap, false, true, false, 4);
      myGoals.clear();
      generateGoals(myMap);
    }
    if(map_received and not RRT_created and GOALS_generated){
      itterations = 0;
      auto start = high_resolution_clock::now();
      generateRRT();
      auto stop = high_resolution_clock::now();
      auto duration = duration_cast<microseconds>(stop - start);
      if(itterations != 100000){
        cout << "\nExecution time: " << duration.count() << " micro seconds for " << itterations << " itterations." << endl;
      }else{
        cout << "\nTimeout after " << duration.count() << " micro seconds for " << itterations << " itterations." << endl;
      }
      auto start2 = high_resolution_clock::now();
      findShortestPath();
      auto stop2 = high_resolution_clock::now();
      auto duration2 = duration_cast<microseconds>(stop2 - start2);
      cout << "\nExecution time: " << duration2.count() << ", this is to find the shortest paths to our goals" << endl;
      RRT_created = true;
      itterations = 0;
    }

    if(map_received and RRT_created){
      visualization_msgs::Marker RRT_points, RRT_line_list, PATH_points, PATH_line_list, GOAL_points, HITS_points;
    
      RRT_points.header.frame_id = RRT_line_list.header.frame_id = "odom_shafter";
      RRT_points.ns = "points";
      RRT_points.action = visualization_msgs::Marker::ADD;
      RRT_points.pose.orientation.w = 1.0;
      RRT_points.id = 0;
      RRT_line_list.id = 1;
      RRT_points.type = visualization_msgs::Marker::POINTS;
      RRT_line_list.type = visualization_msgs::Marker::LINE_LIST;
      RRT_points.scale.x = 0.2;
      RRT_points.scale.y = 0.2;
      RRT_line_list.scale.x = 0.1;
      RRT_points.color.g = 1.0f;
      RRT_points.color.a = 1.0;
      RRT_line_list.color.b = 1.0;
      RRT_line_list.color.a = 1.0;
      std::list<node>::iterator it_comeon_visualizer;	
      for(it_comeon_visualizer = RRT_TREE.begin(); it_comeon_visualizer != RRT_TREE.end(); it_comeon_visualizer++){
        geometry_msgs::Point p;
        p.x = it_comeon_visualizer->point->x();
        p.y = it_comeon_visualizer->point->y();
        p.z = it_comeon_visualizer->point->z();
        RRT_points.points.push_back(p);
        if(it_comeon_visualizer->myParent != nullptr){
          RRT_line_list.points.push_back(p);
          p.x = it_comeon_visualizer->myParent->point->x();
          p.y = it_comeon_visualizer->myParent->point->y();
          p.z = it_comeon_visualizer->myParent->point->z();
          RRT_line_list.points.push_back(p);
        }
      }
      points_pub.publish(RRT_points);
      points_pub.publish(RRT_line_list);
      
      if(!fetched_path and RRT_created){
      fetched_path = true;
      PATH_points.header.frame_id = PATH_line_list.header.frame_id = "odom_shafter";
      PATH_points.ns = "points";
      PATH_points.action = visualization_msgs::Marker::ADD;
      PATH_points.pose.orientation.w = 1.0;
      PATH_points.id = 0;
      PATH_line_list.id = 1;
      PATH_points.type = visualization_msgs::Marker::POINTS;
      PATH_line_list.type = visualization_msgs::Marker::LINE_LIST;
      PATH_points.scale.x = 0.2;
      PATH_points.scale.y = 0.2;
      PATH_line_list.scale.x = 0.1;
      PATH_points.color.g = 1.0f;
      PATH_points.color.a = 1.0;
      PATH_line_list.color.b = 1.0;
      PATH_line_list.color.a = 1.0;
      auto start = high_resolution_clock::now();
      setPath();
      PATH.clear();
      goalNode->getPath(&PATH);
      auto stop = high_resolution_clock::now();
      auto duration = duration_cast<microseconds>(stop - start);
      cout << "\nExecution time: " << duration.count() << " micro seconds for " << myGoals.size() << " path." << endl;
      PATH.push_back(goalNode);
      std::list<node*>::iterator it_comeon_visualizer2;	
      for(it_comeon_visualizer2 = PATH.begin(); it_comeon_visualizer2 != PATH.end(); it_comeon_visualizer2++){
        geometry_msgs::Point p;
        p.x = (*it_comeon_visualizer2)->point->x();
        p.y = (*it_comeon_visualizer2)->point->y();
        p.z = (*it_comeon_visualizer2)->point->z();
        PATH_points.points.push_back(p);
        if((*it_comeon_visualizer2)->myParent != nullptr){
          PATH_line_list.points.push_back(p);
          p.x = (*it_comeon_visualizer2)->myParent->point->x();
          p.y = (*it_comeon_visualizer2)->myParent->point->y();
          p.z = (*it_comeon_visualizer2)->myParent->point->z();
          PATH_line_list.points.push_back(p);
        }
      }
      path_pub.publish(PATH_points);
      path_pub.publish(PATH_line_list);
      }
      
      GOAL_points.header.frame_id = "odom_shafter";
      GOAL_points.ns = "points";
      GOAL_points.action = visualization_msgs::Marker::ADD;
      GOAL_points.pose.orientation.w = 1.0;
      GOAL_points.id = 0;
      GOAL_points.type = visualization_msgs::Marker::POINTS;
      GOAL_points.scale.x = 0.2;
      GOAL_points.scale.y = 0.2;
      GOAL_points.color.r = 1.0f;
      GOAL_points.color.a = 1.0;
      std::list<node>::iterator it_comeon_visualizer3;	
      for(it_comeon_visualizer3 = myGoals.begin(); it_comeon_visualizer3 != myGoals.end(); it_comeon_visualizer3++){
        geometry_msgs::Point p;
        p.x = it_comeon_visualizer3->point->x();
        p.y = it_comeon_visualizer3->point->y();
        p.z = it_comeon_visualizer3->point->z();
        GOAL_points.points.push_back(p);
      }
      goal_pub.publish(GOAL_points);
      
      hits.clear();
      goalNode->addHits(&hits);
      HITS_points.header.frame_id = "odom_shafter";
      HITS_points.ns = "points";
      HITS_points.action = visualization_msgs::Marker::ADD;
      HITS_points.pose.orientation.w = 1.0;
      HITS_points.id = 0;
      HITS_points.type = visualization_msgs::Marker::POINTS;
      HITS_points.scale.x = 0.2;
      HITS_points.scale.y = 0.2;
      HITS_points.color.r = 1.0f;
      HITS_points.color.a = 1.0;
      std::list<ufo::math::Vector3>::iterator it_comeon_visualizer4;	
      for(it_comeon_visualizer4 = hits.begin(); it_comeon_visualizer4 != hits.end(); it_comeon_visualizer4++){
        geometry_msgs::Point p;
        p.x = it_comeon_visualizer4->x();
        p.y = it_comeon_visualizer4->y();
        p.z = it_comeon_visualizer4->z();
        HITS_points.points.push_back(p);
      }
      hits_pub.publish(HITS_points);
      
      ufomap_msgs::UFOMapStamped::Ptr msg(new ufomap_msgs::UFOMapStamped);
      bool compress = false;
      ufo::map::DepthType pub_depth = 0;
      // Convert UFOMap to ROS message
    if (ufomap_msgs::ufoToMsg(myMap, msg->map, compress, pub_depth)) {
      //std::cout << "Map conversion success!" << std::endl;
      // Conversion was successful
      msg->header.stamp = ros::Time::now();
      msg->header.frame_id = "odom_shafter";
      map_pub.publish(msg);					        
    }else{
      std::cout << "Map conversion failed!" << std::endl;
    }
    itterations++;
    if(itterations > 100){
      itterations = 0;
      fetched_path = false;
      RRT_created = false;
      GOALS_generated = false;
    }
    }
    ros::spinOnce();
    rate.sleep();
  }
  return 0;
}
