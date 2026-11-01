// A character that walks with WASD, falls under gravity and jumps with Space, and can't walk through walls.
//
// Setup: give the entity a Collider and a RigidBody set to KINEMATIC, then attach this script.
// (Not Dynamic: a Dynamic body is moved by the simulation and Entity.Move refuses to touch it.)
class Walker
{
    Entity entity;

    float speed     = 4.0f;    // units per second
    float gravity   = 20.0f;
    float jumpSpeed = 8.0f;

    float verticalSpeed = 0.0f;

    void OnUpdate(float dt)
    {
        // Which way the player wants to go, relative to where the entity is facing.
        Vec3 direction(0, 0, 0);
        if (Input::IsKeyDown(Key::W)) direction = direction + entity.Forward();
        if (Input::IsKeyDown(Key::S)) direction = direction - entity.Forward();
        if (Input::IsKeyDown(Key::D)) direction = direction + entity.Right();
        if (Input::IsKeyDown(Key::A)) direction = direction - entity.Right();

        // Sideways first. Walking into a wall at an angle slides along it instead of sticking.
        if (direction.Length() > 0.0f)
            entity.Move(direction.Normalized() * (speed * dt));

        // Then up/down on its own, so a wall can't swallow the vertical movement (or the other way round).
        verticalSpeed -= gravity * dt;
        bool hitSomething = entity.Move(Vec3(0, verticalSpeed * dt, 0));

        if (hitSomething)
        {
            // We were moving down, so we landed; we were moving up, so we bumped a ceiling.
            bool landed = verticalSpeed < 0.0f;
            verticalSpeed = 0.0f;

            if (landed && Input::IsKeyDown(Key::Space))
                verticalSpeed = jumpSpeed;
        }
    }
}
