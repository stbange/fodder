local function ruin(x, z, half_x, half_z, height)
    return { x = x, z = z, half_x = half_x, half_z = half_z, height = height }
end

local arena = {
    name = "The Broken Ford",
    width = 56, length = 60, channel = 5, ford = 10, dip = 0.85,
    obstacles = {
        ruin(-24, 5, 3.9, 0.85, 3.8),
        ruin(-28.3, 0, 0.9, 5.2, 3.4),
        ruin(-19, -3.6, 0.85, 2.85, 2.5),
        ruin(-22.2, -6.8, 2.5, 0.7, 3.3),
        ruin(19.1, 4.2, 2.2, 1.2, 3.2),
        ruin(23.2, -2.5, 1.1, 3.5, 3.6),
        ruin(34, 7, 2, 1.8, 2.4),
        ruin(34.5, -7, 1.75, 2.6, 4.0),
        ruin(-43.4, -2, 2.25, 2.5, 3.1),
        ruin(-7.6, -8.7, 0.7, 0.7, 2.2),
        ruin(7.6, 8.7, 0.7, 0.7, 2.4)
    }
}

local function battle(name, hint, seconds, fog, ally_lane, enemy_lane, hostile)
    return {
        name = name, hint = hint, seconds = seconds, rout = 0.29, fog = fog,
        ally_lane = ally_lane, enemy_lane = enemy_lane,
        ally_speed = 1.60, enemy_speed = 1.63 + hostile * 0.14,
        ally_damage = 6.4, enemy_damage = 5.5 + hostile * 1.2, player_damage = 49
    }
end

local function gain(rank)
    return rank / (rank + 3)
end

return {
    arena = arena,
    battles = {
        battle("Hold the Ford", "Advance with the Reed Guard or cut through the ruins.", 280, 93, 0, 0, 0),
        battle("Fog on the Water", "The Cinder Host presses into the western ruins.", 300, 48, -1, -1, 1),
        battle("The Last Crossing", "An eastward push leaves the center exposed.", 340, 76, 0, 1, 2)
    },
    upgrades = {
        { name = "Second Wind", description = "Recover stamina faster every battle." },
        { name = "Iron Buckle", description = "Spend less stamina on successful blocks." },
        { name = "Field Dressing", description = "Recover health once when badly wounded." }
    },
    effects = function(wind, buckle, dressing)
        return {
            recovery = 1 + 0.55 * gain(wind),
            block_cost = 1 - 0.46 * gain(buckle),
            dressing = 38 * gain(dressing)
        }
    end
}
