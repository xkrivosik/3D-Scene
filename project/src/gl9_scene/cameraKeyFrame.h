struct CameraKeyframe {
    float time;                    // čas v sekundách
    glm::vec3 position;            // kde je kamera
    glm::vec3 lookAt;              // kam sa pozerá (cieľový bod)
};

std::vector<CameraKeyframe> camKeyframes = {
    {  0.0f, glm::vec3(-7.0f,  2.0f,  30.0f), glm::vec3(0.0f, 0.5f, 0.0f) },
    {  10.0f, glm::vec3( -7.0f,  2.0f,  3.0f), glm::vec3(0.0f, 0.5f, 1.0f) },
    {  20.0f, glm::vec3( -1.2f,  1.5f,  7.0f), glm::vec3(0.0f, 0.5f, 1.0f) },
    {  30.0f, glm::vec3( 3.0f,  2.0f,  3.0f), glm::vec3(0.0f, 0.5f, 1.0f) },
    {  40.0f, glm::vec3( 2.0f,  2.0f,  -2.0f), glm::vec3(0.0f, 0.5f, 1.0f) },
    {  50.0f, glm::vec3( 0.0f,  0.5f,  -2.0f), glm::vec3(0.0f, 0.5f, 1.0f) },
    {  60.0f, glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.3f, 1.5f) },
    {  70.0f, glm::vec3(1.8f, 0.3f, 2.0f), glm::vec3(1.0f, 0.0f, 1.5f) },
    {  80.0f, glm::vec3(-1.0f, 1.5f, 3.2f), glm::vec3(1.0f, 0.7f, 2.5f) },
    {  90.0f, glm::vec3(-1.0f, 1.5f, 3.2f), glm::vec3(0.0f, 0.7f, -7.0f) },
    {  100.0f, glm::vec3(-1.2f, 1.3f, -1.5f), glm::vec3(0.0f, 0.8f, 7.0f) },
    {  110.0f, glm::vec3(-1.2f, 2.0f, -3.0f), glm::vec3(0.5f, 0.5f, 7.0f) },
    {  120.0f, glm::vec3(-7.0f,  2.0f,  -3.0f), glm::vec3(-22.0f,  2.0f,  -6.4f) },
    {125.0f, glm::vec3(-14.8f,  1.0f,  -7.0f), glm::vec3(-31.0f,  1.0f,  6.3f) },
    {135.0f, glm::vec3(-14.8f,  1.0f,  17.0f), glm::vec3(-31.0f,  1.0f,  6.3f) },
    {140.0f, glm::vec3(-25.0f,  1.0f,  17.5f), glm::vec3(-24.5f,  1.0f,  -10.0f) },
    {150.0f, glm::vec3(-29.0f,  1.0f,  4.6f), glm::vec3(-18.0f,  -3.0f,  4.6f) },
    {160.0f, glm::vec3(-22.0f,  1.0f,  -6.4f), glm::vec3(-18.0f,  -3.0f,  4.6f) },
    {165.0f, glm::vec3(-17.0f,  1.0f,  -8.5f), glm::vec3(-18.0f,  0.0f,  4.6f) },
    {170.0f, glm::vec3(-7.0f,  1.0f,  -8.5f), glm::vec3(-7.0f,  2.0f,  90.0f) },
    {175.0f, glm::vec3(-7.0f,  2.0f,  30.0f), glm::vec3(-7.0f,  2.0f,  90.0f) },

};

float camAnimDuration = camKeyframes.back().time;
float camCurrentTime = 0.0f;