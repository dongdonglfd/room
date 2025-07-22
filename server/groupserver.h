#include "Friendserver.h"
// #include "groupchatserver.h"
class groupserver: public groupchat
{
    public:
    Friendserver mysql;
    
    void handleCreateGroup(int fd, const json& req)
    {
        string user = req["user"];
        string groupName = req["group_name"];
        
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement("INSERT INTO `groups` (name, creator) VALUES (?, ?)")
        );
        
        json response;
        try {
            stmt->setString(1, groupName);
            stmt->setString(2, user);
            stmt->executeUpdate();
            
            // 获取新群ID
            unique_ptr<sql::Statement> getIdStmt(con->createStatement());
            unique_ptr<sql::ResultSet> res(
                getIdStmt->executeQuery("SELECT LAST_INSERT_ID() AS id")
            );
            
            if (res->next()) {
                int groupId = res->getInt("id");
                
                // 将创建者添加为群主
                unique_ptr<sql::PreparedStatement> addOwnerStmt(
                    con->prepareStatement(
                        "INSERT INTO group_members (group_id, user, role) "
                        "VALUES (?, ?, 'owner')"
                    )
                );
                addOwnerStmt->setInt(1, groupId);
                addOwnerStmt->setString(2, user);
                addOwnerStmt->executeUpdate();
                
                response["success"] = true;
                response["group_id"] = groupId;
            } else {
                response["success"] = false;
                response["message"] = "创建群组失败";
            }
        } catch(sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleGetMyGroups(int fd, const json& req) 
    {
        string user = req["user"];
        
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        
        try {
            unique_ptr<sql::PreparedStatement> stmt(
                con->prepareStatement(
                    "SELECT g.id, g.name, gm.role "
                    "FROM `groups` g "
                    "JOIN group_members gm ON g.id = gm.group_id "
                    "WHERE gm.user = ?"
                )
            );
            stmt->setString(1, user);
            unique_ptr<sql::ResultSet> res(stmt->executeQuery());
            
            json groups = json::array();
            while (res->next()) {
                groups.push_back({
                    {"id", res->getInt("id")},
                    {"name", res->getString("name")},
                    {"role", res->getString("role")}
                });
            }
            
            response["success"] = true;
            response["groups"] = groups;
        } catch(sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleDisbandGroup(int fd,const json& req)
    {
        string user=req["user"];
        int groupId=req["group_id"];

        unique_ptr<sql::Connection> con (mysql.getDBConnection());
        json response;
        try{
            unique_ptr<sql::PreparedStatement> checkRoleStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
        );
        checkRoleStmt->setInt(1, groupId);
        checkRoleStmt->setString(2, user);
        unique_ptr<sql::ResultSet> resRole(checkRoleStmt->executeQuery());
        
        if (!resRole->next() || resRole->getString("role") != "owner") {
            response["success"] = false;
            response["message"] = "仅群主可以解散群组";
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }
        
        // 删除群组（级联删除成员和消息）
        unique_ptr<sql::PreparedStatement> delStmt(
            con->prepareStatement("DELETE FROM `groups` WHERE id = ?")
        );
        delStmt->setInt(1, groupId);
        int affected = delStmt->executeUpdate();
        
        if (affected > 0) {
            response["success"] = true;
            response["message"] = "群组已解散";
        } else {
            response["success"] = false;
            response["message"] = "群组不存在";
        }
        }catch(sql::SQLException &e) {
        response["success"] = false;
        response["message"] = "数据库错误: " + string(e.what());
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);

    }
    void handleLeaveGroup(int fd,const json& req)
    {
        string user = req["user"];
        int groupId = req["group_id"];
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        try {
        // 1. 检查用户是否在群组中
        unique_ptr<sql::PreparedStatement> checkStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
        );
        checkStmt->setInt(1, groupId);
        checkStmt->setString(2, user);
        unique_ptr<sql::ResultSet> res(checkStmt->executeQuery());
        
        if (!res->next()) {
            response["success"] = false;
            response["message"] = "您不在该群组中";
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }
        
        string role = res->getString("role");
        
        // 2. 检查用户是否是群主（群主不能直接退出，需先解散群组或转让群主）
        if (role == "owner") {
            response["success"] = false;
            response["message"] = "群主不能直接退出群组，请先转让群主或解散群组";
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }
        
        // 3. 执行退出操作
        unique_ptr<sql::PreparedStatement> leaveStmt(
            con->prepareStatement(
                "DELETE FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
        );
        leaveStmt->setInt(1, groupId);
        leaveStmt->setString(2, user);
        
        int affected = leaveStmt->executeUpdate();
        
        if (affected > 0) {
            // 如果该用户是管理员，需要移除其管理员身份
            // if (role == "admin") {
            //     unique_ptr<sql::PreparedStatement> adminStmt(
            //         con->prepareStatement(
            //             "DELETE FROM group_admins "
            //             "WHERE group_id = ? AND user = ?"
            //         )
            //     );
            //     adminStmt->setInt(1, groupId);
            //     adminStmt->setString(2, user);
            //     adminStmt->executeUpdate();
            // }
            
            response["success"] = true;
            response["message"] = "已退出群组";
        } else {
            response["success"] = false;
            response["message"] = "退出失败";
        }
        } catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
        }
    
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleGetUserRole(int fd, const json& req)
    {
        int groupId = req["group_id"];
        string user = req["user"];
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        try{
            unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
            );
            stmt->setInt(1, groupId);
            stmt->setString(2, user);
        
            unique_ptr<sql::ResultSet> res(stmt->executeQuery());
            if(res->next()){
                response["success"]=true;
                response["role"]=res->getString("role");
            }else {
                response["success"] = false;
                response["role"] = "none";
            }


        }catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
        }
        
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    } 
    void handleJoinGroupRequest(int fd, const json& req)
    {
        string user = req["user"];
        int groupId = req["group_id"];
        string message = req["message"];
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        unique_ptr<sql::PreparedStatement> checkGroupStmt(
            con->prepareStatement("SELECT id FROM `groups` WHERE id = ?")
        );
        checkGroupStmt->setInt(1,groupId);
        unique_ptr<sql::ResultSet> groupRes(checkGroupStmt->executeQuery());
        if (!groupRes->next()) {
            response["success"] = false;
            response["message"] = "群组不存在";
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }
        unique_ptr<sql::PreparedStatement> checkMemberStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
        );
        checkMemberStmt->setInt(1, groupId);
        checkMemberStmt->setString(2, user);
        unique_ptr<sql::ResultSet> memberRes(checkMemberStmt->executeQuery());
        
        if (memberRes->next()) {
            string role = memberRes->getString("role");
            response["success"] = false;
            response["message"] = "您已经是群成员（角色: " + role + ")";
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }
        unique_ptr<sql::PreparedStatement> checkRequestStmt(
            con->prepareStatement(
                "SELECT id FROM group_join_requests "
                "WHERE group_id = ? AND user = ? "
            )
        );
        checkRequestStmt->setInt(1, groupId);
        checkRequestStmt->setString(2, user);
        unique_ptr<sql::ResultSet> requestRes(checkRequestStmt->executeQuery());
        
        if (requestRes->next()) {
            response["success"] = false;
            response["message"] = "您已经有一个待处理的加群请求";
            send(fd, response.dump().c_str(), response.dump().size(), 0);
            return;
        }
        unique_ptr<sql::PreparedStatement> insertStmt(
            con->prepareStatement(
                "INSERT INTO group_join_requests "
                "(group_id, user, message) "
                "VALUES (?, ?, ?)"
            )
        );
        insertStmt->setInt(1, groupId);
        insertStmt->setString(2, user);
        insertStmt->setString(3, message);
        insertStmt->executeUpdate();
        response["success"] = true;
        response["message"] = "加群请求已发送";
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleGetGroupJoinRequests(int fd, const json& req) 
    {
        int groupId = req["group_id"];
        string admin = req["admin"];
        
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        
        try {
            unique_ptr<sql::PreparedStatement> stmt(
                con->prepareStatement(
                    // "SELECT r.id, r.user, r.message, r.created_at, "
                    // "u.email, u.qq, u.created_at AS user_created "
                    // "FROM group_join_requests r "
                    // "JOIN users u ON r.user = u.username "
                    // "WHERE r.group_id = ? AND r.status = 'pending' "
                    // "ORDER BY r.created_at ASC"
                    "SELECT  id, user, message FROM group_join_requests WHERE group_id = ?"
                )
            );
            stmt->setInt(1, groupId);
            
            unique_ptr<sql::ResultSet> res(stmt->executeQuery());
            json requests = json::array();
            
            while (res->next()) {
                json request = {
                    {"id", res->getInt("id")},
                    {"user", res->getString("user")},
                    {"message", res->getString("message")},
                    // {"created_at", res->getString("created_at")},
                    // {"user_info", {
                    //     {"email", res->getString("email")},
                    //     {"qq", res->getString("qq")},
                    //     {"join_date", res->getString("user_created")}
                    // }}
                };
                requests.push_back(request);
            }
            
            response["success"] = true;
            response["requests"] = requests;
            
        } catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
            cerr << "SQL Exception: " << e.what() << endl;
        }
        
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleProcessJoinRequest(int fd, const json& req)
    {
        int groupId = req["group_id"];
        string admin = req["admin"];
        string username = req["user"];
        string action = req["action"];
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
            json response;
        if (action == "accept") {
            
            unique_ptr<sql::PreparedStatement> addMemberStmt(
                con->prepareStatement(
                    "INSERT INTO group_members (group_id, user, role) "
                    "VALUES (?, ?, 'member') "
                    "ON DUPLICATE KEY UPDATE role = VALUES(role)"
                )
            );
            addMemberStmt->setInt(1, groupId);
            addMemberStmt->setString(2, username);
            addMemberStmt->executeUpdate();
            
        }
        // else if (action == "reject")
        unique_ptr<sql::PreparedStatement> delStmt(
            con->prepareStatement("DELETE FROM group_join_requests WHERE group_id = ? AND user = ?")
            );
            delStmt->setInt(1, groupId);
            delStmt->setString(2, username);
            int affected = delStmt->executeUpdate();
        response["success"] = true;
        response["message"] = (action == "accept" ? "已批准加入" : "已拒绝加入");
        send(fd, response.dump().c_str(), response.dump().size(), 0);

    }
    void handleGetGroupInfo(int fd, const json& req) 
    {
        int groupId = req["group_id"];
        
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        
        try {
            // 1. 获取群组基本信息
            unique_ptr<sql::PreparedStatement> groupStmt(
                con->prepareStatement(
                    "SELECT id, name, creator, created_at "
                    "FROM `groups` "
                    "WHERE id = ?"
                )
            );
            groupStmt->setInt(1, groupId);
            
            unique_ptr<sql::ResultSet> groupRes(groupStmt->executeQuery());
            if (!groupRes->next()) {
                response["success"] = false;
                response["message"] = "群组不存在";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            
            // 2. 获取管理员列表
            unique_ptr<sql::PreparedStatement> adminStmt(
                con->prepareStatement(
                    "SELECT user "
                    "FROM group_members "
                    "WHERE group_id = ? AND (role = 'owner' OR role = 'admin')"
                )
            );
            adminStmt->setInt(1, groupId);
            unique_ptr<sql::ResultSet> adminRes(adminStmt->executeQuery());
            json adminArr = json::array();
            while (adminRes->next()) {
                adminArr.push_back(adminRes->getString("user"));
            }
            
            // 3. 获取普通成员列表
            unique_ptr<sql::PreparedStatement> memberStmt(
                con->prepareStatement(
                    "SELECT user "
                    "FROM group_members  "
                    "WHERE group_id = ? AND role = 'member' "
                )
            );
            memberStmt->setInt(1, groupId);
            unique_ptr<sql::ResultSet> memberRes(memberStmt->executeQuery());
            json memberArr = json::array();
            while (memberRes->next()) {
                memberArr.push_back(memberRes->getString("user"));
            }
            
            // 4. 统计总成员数
            int totalMembers = adminArr.size() + memberArr.size();
            
            
            //构建响应
            response["success"] = true;
            response["id"] = groupId;
            response["name"] = groupRes->getString("name");
            response["creator"] = groupRes->getString("creator");
            response["created_at"] = groupRes->getString("created_at");
            response["member_count"] = totalMembers;
            response["admins"] = adminArr;
            response["members"] = memberArr;
            
        } catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
            cerr << "SQL Exception: " << e.what() << endl;
        }
        
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleAddGroupAdmin(int fd, const json& req)
    {
        int groupId = req["group_id"];
        string username = req["user"];
        string adminUsername = req["admin"]; // 操作者用户名
        
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        bool success = false;
        try{
            unique_ptr<sql::PreparedStatement> authStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
            );
            authStmt->setInt(1, groupId);
            authStmt->setString(2, adminUsername);
            unique_ptr<sql::ResultSet> authRes(authStmt->executeQuery());
            
            if (!authRes->next()) {
                response["success"] = false;
                response["message"] = "操作者不在该群组中";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            
            string adminRole = authRes->getString("role");
            if (adminRole != "owner") {
                response["success"] = false;
                response["message"] = "只有群主可以添加管理员";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            unique_ptr<sql::PreparedStatement> userStmt(
                con->prepareStatement("SELECT username FROM users WHERE username = ?")
            );
            userStmt->setString(1, username);
            unique_ptr<sql::ResultSet> userRes(userStmt->executeQuery());
            
            if (!userRes->next()) {
                response["success"] = false;
                response["message"] = "用户不存在: " + username;
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            
            unique_ptr<sql::PreparedStatement> memberStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
            );
            memberStmt->setInt(1, groupId);
            memberStmt->setString(2, username);
            unique_ptr<sql::ResultSet> memberRes(memberStmt->executeQuery());
            
            if (!memberRes->next()) {
                response["success"] = false;
                response["message"] = "用户 " + username + " 不在群组中";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            string currentRole = memberRes->getString("role");
            if (currentRole == "admin" || currentRole == "owner") {
                response["success"] = false;
                response["message"] = "用户已经是管理员或群主";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            unique_ptr<sql::PreparedStatement> updateStmt(
            con->prepareStatement(
                "UPDATE group_members SET role = 'admin' "
                "WHERE group_id = ? AND user = ?"
            )
            );
            updateStmt->setInt(1, groupId);
            updateStmt->setString(2, username);
            int affectedRows = updateStmt->executeUpdate();
            
            if (affectedRows == 0) {
                response["success"] = false;
                response["message"] = "设置管理员失败，请稍后重试";
            } else {
                response["success"] = true;
                response["message"] = "已成功设置 " + username + " 为管理员";
                success = true; 
            }

        }catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
            cerr << "SQL Exception: " << e.what() << endl;
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleRemoveGroupAdmin(int fd, const json& req)
    {
        int groupId = req["group_id"];
        string username = req["user"];
        string adminUsername = req["admin"]; // 操作者用户名
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        bool success = false;
        try{
            unique_ptr<sql::PreparedStatement> authStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
            );
            authStmt->setInt(1, groupId);
            authStmt->setString(2, adminUsername);
            unique_ptr<sql::ResultSet> authRes(authStmt->executeQuery());
            
            if (!authRes->next()) {
                response["success"] = false;
                response["message"] = "操作者不在该群组中";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            string adminRole = authRes->getString("role");
            if (adminRole != "owner") {
                response["success"] = false;
                response["message"] = "只有群主可以移除管理员";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            unique_ptr<sql::PreparedStatement> memberStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
            );
            memberStmt->setInt(1, groupId);
            memberStmt->setString(2, username);
            unique_ptr<sql::ResultSet> memberRes(memberStmt->executeQuery());
            
            if (!memberRes->next()) {
                response["success"] = false;
                response["message"] = "用户 " + username + " 不在群组中";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
             unique_ptr<sql::PreparedStatement> updateStmt(
            con->prepareStatement(
                "UPDATE group_members SET role = 'member'"
                "WHERE group_id = ? AND user = ?"
            )
            );
            updateStmt->setInt(1, groupId);
            updateStmt->setString(2, username);
            int affectedRows = updateStmt->executeUpdate();
            
            if (affectedRows == 0) {
                response["success"] = false;
                response["message"] = "移除管理员失败，请稍后重试";
            } else {
                response["success"] = true;
                response["message"] = "已成功移除 " + username + " 的管理员身份";
                success = true;
            }



        }catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
            cerr << "SQL Exception: " << e.what() << endl;
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handleRemoveGroupMember(int fd, const json& req)
    {
        int groupId = req["group_id"];
        string username = req["user"];
        string adminUsername = req["admin"]; // 操作者用户名
        
        unique_ptr<sql::Connection> con(mysql.getDBConnection());
        json response;
        bool success = false;
        try{
            unique_ptr<sql::PreparedStatement> authStmt(
            con->prepareStatement(
                "SELECT role FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
            );
            authStmt->setInt(1, groupId);
            authStmt->setString(2, adminUsername);
            unique_ptr<sql::ResultSet> authRes(authStmt->executeQuery());
            
            if (!authRes->next()) {
                response["success"] = false;
                response["message"] = "您不是该群组成员，无权限操作";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            
            string adminRole = authRes->getString("role");
            if (adminRole != "owner" && adminRole != "admin") {
                response["success"] = false;
                response["message"] = "只有管理员或群主可以移除成员";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            
            // 2. 验证被移除用户身份
            if (username == adminUsername) {
                response["success"] = false;
                response["message"] = "不能移除自己，如需退群请使用离开群功能";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            // 3. 检查目标用户是否在群组中
            unique_ptr<sql::PreparedStatement> targetStmt(
                con->prepareStatement(
                    "SELECT role FROM group_members "
                    "WHERE group_id = ? AND user = ?"
                )
            );
            targetStmt->setInt(1, groupId);
            targetStmt->setString(2, username);
            unique_ptr<sql::ResultSet> targetRes(targetStmt->executeQuery());
            
            if (!targetRes->next()) {
                response["success"] = false;
                response["message"] = "用户 " + username + " 不在群组中";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
            // 4. 权限验证：管理员不能移除更高级别成员
            string targetRole = targetRes->getString("role");
            
            // 管理员权限矩阵
            if (adminRole == "admin") {
                if (targetRole == "owner" || targetRole == "admin") {
                    response["success"] = false;
                    response["message"] = "管理员不能移除其他管理员或群主";
                    send(fd, response.dump().c_str(), response.dump().size(), 0);
                    return;
                }
            }
            
            // 群主不能移除自己（已处理）但可以移除其他人
            if (adminRole == "owner" && targetRole == "owner") {
                response["success"] = false;
                response["message"] = "不能移除其他群主，请先处理群主身份";
                send(fd, response.dump().c_str(), response.dump().size(), 0);
                return;
            }
             // 5. 执行移除操作
            unique_ptr<sql::PreparedStatement> deleteStmt(
                con->prepareStatement(
                    "DELETE FROM group_members "
                    "WHERE group_id = ? AND user = ?"
                )
            );
            deleteStmt->setInt(1, groupId);
            deleteStmt->setString(2, username);
            int affectedRows = deleteStmt->executeUpdate();
            
            if (affectedRows == 0) {
                response["success"] = false;
                response["message"] = "移除成员失败，请稍后重试";
            } else {
                response["success"] = true;
                response["message"] = "已成功移除 " + username;
                success = true;
            }

        }catch (sql::SQLException &e) {
            response["success"] = false;
            response["message"] = "数据库错误: " + string(e.what());
            cerr << "SQL Exception: " << e.what() << endl;
        }
        send(fd, response.dump().c_str(), response.dump().size(), 0);

    }
    void handleAckGroupMessage(int client_sock, const json& data)
    {
        if (!data.contains("user") || !data["user"].is_string() ||
        !data.contains("groupid") || !data["groupid"].is_number()) {
        json errorResponse = {
            {"success", false},
            {"message", "无效请求: 缺少必要参数"}
        };
        sendResponse(client_sock, errorResponse);
            return;
        }

        string username = data["user"];
        int groupid = data["groupid"];
        
        unique_ptr<sql::Connection> con(getDBConnection());
        if (!con) {
            json errorResponse = {
                {"success", false},
                {"message", "数据库连接失败"}
            };
            sendResponse(client_sock, errorResponse);
            return;
        }
        unique_ptr<sql::PreparedStatement> stmt(
                con->prepareStatement(
                    "DELETE FROM offline_messages "
                    "WHERE recipient_username = ? AND group_id = ?"
                )
            );
            stmt->setString(1, username);
            stmt->setInt(2, groupid);
            stmt->executeUpdate();
    }
    
        

    

};