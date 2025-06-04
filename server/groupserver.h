#include "Friendserver.h"

// 
// using namespace std;
// using json = nlohmann::json;
// using namespace sql;
// 
//数据库配置
// const string DB_HOST = "tcp://127.0.0.1:3306";
// const string DB_USER = "chatuser";   // 数据库账户名
// const string DB_PASS = "123";  // 数据库账户密码
// const string DB_NAME = "chat";
// 
class groupserver
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
};