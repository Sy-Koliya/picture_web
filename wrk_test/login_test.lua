-- 随机生成中文昵称（常见姓氏 + 随机名）
local function generate_random_nickname()
    local surnames = {"zhang", "wang", "li", "zhao", "liu", "chen", "yang", "huang", "zhou", "wu"}
    local names = {"wei", "fang", "qiang", "min", "lei", "tingting", "yang", "xiaohong", "jianguo", "li"}
    return surnames[math.random(#surnames)] .. names[math.random(#names)]
end

-- 随机生成用户名（字母 + 数字）
local function generate_random_username()
    local charset = "abcdefghijklmnopqrstuvwxyz0123456789"
    local username = ""
    for i = 1, 8 do
        local rand = math.random(#charset)
        username = username .. charset:sub(rand, rand)
    end
    return username
end

local function generate_random_password()
    local charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*"
    local password = ""
    for i = 1, 10 do
        local rand = math.random(#charset)
        password = password .. charset:sub(rand, rand)
    end
    return password
end

-- 初始化随机种子
function init(args)
    math.randomseed(os.time())
end

-- 构建请求
function request()
    local nickname = generate_random_nickname()
    local username = generate_random_username()
    local password = generate_random_password()

    -- 构建 JSON 请求体
    local body = string.format(
        '{"nickName":"%s","userName":"%s","firstPwd":"%s"}',
        nickname, username, password
    )

    -- 设置请求头
    local headers = {
        ["Content-Type"] = "application/json"
    }

    -- 构造 POST 请求
    return wrk.format("POST", "/api/reg", headers, body)
end