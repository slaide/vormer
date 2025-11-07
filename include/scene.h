#pragma once

struct NodeName{
    char name[256];
};
struct Transform2D{
    int _unused;
};
struct Transform3D{
    int _unused;
};
struct Camera3D{
    enum CAMERA3D_KIND{
        CAMERA3D_KIND_PERSPECTIVE,
        CAMERA3D_KIND_ORTHOGRAPHIC,
    }kind;
    union{
        // CAMERA3D_KIND_PERSPECTIVE
        struct{
            float fovy;
            float near;
            float far;
            float aspect;
        }perspective;
        // CAMERA3D_KIND_ORTHOGRAPHIC
        struct{
            float near,far;
            float left,right;
            float top,bottom;
        }orthographic;
    };
};
struct Camera2D{
    enum CAMERA2D_KIND{
        CAMERA2D_KIND_ORTHOGRAPHIC,
    }kind;
    union{
        // CAMERA2D_KIND_ORTHOGRAPHIC
        struct{
            float left,right;
            float top,bottom;
        }orthographic;
    };
};
struct Mesh{
    int _unused;
};
struct Material{
    int _unused;
};
enum NODE_PROPERTY_KIND{
    NODE_PROPERTY_KIND_NAME,
    NODE_PROPERTY_KIND_TRANSFORM_2D,
    NODE_PROPERTY_KIND_TRANSFORM_3D,
    NODE_PROPERTY_KIND_MESH,
    NODE_PROPERTY_KIND_MATERIAL,

    NODE_PROPERTY_KIND_MAX,
};
struct NodeProperty{
    enum NODE_PROPERTY_KIND kind;
    union{
        // NODE_PROPERTY_NAME
        struct NodeName*name;
        // NODE_PROPERTY_TRANSFORM_2D
        struct Transform2D*transform_2d;
        // NODE_PROPERTY_TRANSFORM_3D
        struct Transform3D*transform_3d;
        // NODE_PROPERTY_KIND_MESH
        struct Mesh*mesh;
        // NODE_PROPERTY_KIND_MATERIAL
        struct Material*material;

        void*data;
    };
};
struct Node{
    int id;

    int num_properties;
    struct NodeProperty*properties;

    int num_children;
    struct Node**children;
};
struct NodeName* node_getName(struct Node*node);
struct Transform2D* node_getTransform2d(struct Node*node);
struct Transform3D* node_getTransform3d(struct Node*node);
struct Mesh* node_getMesh(struct Node*node);
struct Material* node_getMaterial(struct Node*node);

void node_setName(struct Node*node,struct NodeName*name);
void node_setTransform2d(struct Node*node,struct Transform2D*transform2d);
void node_setTransform3d(struct Node*node,struct Transform3D*transform3d);
void node_setMesh(struct Node*node,struct Mesh*mesh);
void node_setMaterial(struct Node*node,struct Material*material);

struct Scene{
    struct Node*root_2d;
    struct Node*camera_2d;

    struct Node*root_3d;
    struct Node*camera_3d;
};
void Scene_setCamera2D(struct Scene*scene,struct Node*camera);
void Scene_setCamera3D(struct Scene*scene,struct Node*camera);
