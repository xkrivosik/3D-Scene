struct Keyframe {
    float time;          // Čas keyframu (v sekundách, začína od 0)
    glm::vec3 position;  // Pozícia (x, y, z)
    glm::vec3 rotation;  // Rotácia (Eulerove uhly: x, y, z v stupňoch)
};