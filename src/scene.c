#include<stdlib.h>

#include<util.h>
#include<scene.h>

struct NodeName* node_getName(struct Node*node){
    for(int i=0;i<node->num_properties;i++){
        if(node->properties[i].kind==NODE_PROPERTY_KIND_NAME)
            return node->properties[i].name;
    }
    return nullptr;
}
struct Transform2D* node_getTransform2d(struct Node*node){
    for(int i=0;i<node->num_properties;i++){
        if(node->properties[i].kind==NODE_PROPERTY_KIND_TRANSFORM_2D)
            return node->properties[i].transform_2d;
    }
    return nullptr;
}
struct Transform3D* node_getTransform3d(struct Node*node){
    for(int i=0;i<node->num_properties;i++){
        if(node->properties[i].kind==NODE_PROPERTY_KIND_TRANSFORM_3D)
            return node->properties[i].transform_3d;
    }
    return nullptr;
}
struct Mesh* node_getMesh(struct Node*node){
    for(int i=0;i<node->num_properties;i++){
        if(node->properties[i].kind==NODE_PROPERTY_KIND_MESH)
            return node->properties[i].mesh;
    }
    return nullptr;
}
struct Material* node_getMaterial(struct Node*node){
    for(int i=0;i<node->num_properties;i++){
        if(node->properties[i].kind==NODE_PROPERTY_KIND_MATERIAL)
            return node->properties[i].material;
    }
    return nullptr;
}
/// set property (copies property argument)
static inline void node_setProperty(struct Node*node,struct NodeProperty*property){
    bool propertyExists=false;
    for(int i=0;i<node->num_properties;i++){
        if(node->properties[i].kind==property->kind){
            propertyExists=true;
        }
    }
    CHECK(!propertyExists,"attempt to insert property %d into node that already has this property\n",property->kind);

    node->properties=realloc(node->properties,(node->num_properties+1)*sizeof(struct NodeProperty));
    node->properties[node->num_properties]=*property;
    node->num_properties++;
}
void node_setName(struct Node*node,struct NodeName*name){
    struct NodeProperty property={
        .kind=NODE_PROPERTY_KIND_NAME,
        .name=name
    };
    node_setProperty(node,&property);
}
void node_setTransform2d(struct Node*node,struct Transform2D*transform2d){
    struct NodeProperty property={
        .kind=NODE_PROPERTY_KIND_TRANSFORM_2D,
        .transform_2d=transform2d
    };
    node_setProperty(node,&property);
}
void node_setTransform3d(struct Node*node,struct Transform3D*transform3d){
    struct NodeProperty property={
        .kind=NODE_PROPERTY_KIND_TRANSFORM_3D,
        .transform_3d=transform3d
    };
    node_setProperty(node,&property);
}
void node_setMesh(struct Node*node,struct Mesh*mesh){
    struct NodeProperty property={
        .kind=NODE_PROPERTY_KIND_MESH,
        .mesh=mesh
    };
    node_setProperty(node,&property);
}
void node_setMaterial(struct Node*node,struct Material*material){
    struct NodeProperty property={
        .kind=NODE_PROPERTY_KIND_MATERIAL,
        .material=material
    };
    node_setProperty(node,&property);
}
