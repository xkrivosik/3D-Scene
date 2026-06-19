// rigid_body.h
#pragma once
#include <glm/glm.hpp>

struct RigidBody {
    glm::vec3 position;
    glm::vec3 velocity = glm::vec3(0);           // lineárna rýchlosť
    glm::vec3 angularVelocity = glm::vec3(0);    // uhlová rýchlosť (rad/s)
    glm::vec3 rotation = glm::vec3(-90,0,-90);   // rotácia v stupňoch

    glm::vec3 force = glm::vec3(0);              // akumulované sily
    glm::vec3 torque = glm::vec3(0);             // akumulovaný moment

    float mass = 1.0f;
    float restitution = 0.3f;                    // odraz
    float drag = 0.98f;                          // odpor prostredia

    void applyForce(const glm::vec3& f, const glm::vec3& atPoint = glm::vec3(0)) {
        force += f;
        if (atPoint != glm::vec3(0)) {
            torque += glm::cross(atPoint - position, f); // moment z excentrického nárazu
        }
    }

    void update(float dt) {
        // lineárna dynamika
        glm::vec3 accel = force / mass;
        velocity += accel * dt;
        velocity *= drag;                // tlmenie
        position += velocity * dt;

        // uhlová dynamika (I = 1)
        glm::vec3 angularAccel = torque;
        angularVelocity += angularAccel * dt;
        angularVelocity *= 0.99f;        // tlmenie rotácie
        rotation += glm::degrees(angularVelocity) * dt;

        force = glm::vec3(0);
        torque = glm::vec3(0);
    }

    // kolízia s podlahou
    void checkFloorCollision() {
        if (position.y < 0.1f) {
            position.y = 0.1f;
            velocity.y = -velocity.y * restitution;

            glm::vec3 offset = glm::vec3(0.5f, 0, 0);
            applyForce(glm::vec3(0, 60.0f, 0), position + offset);
        }
    }
};
