# include <string>
# include <vector>
# include <sys/types.h>
# include <sys/socket.h>
# include <netdb.h>
# include <iostream>
# include <string.h>

# include "webserv/loadConfiguration.hpp"
# include "webserv/request.hpp"
# include "webserv/client.hpp"
# include "webserv/context.hpp"
# include "webserv/http.hpp"

#include "../../include/webserv/request.hpp"
#include "../../include/webserv/context.hpp"
#include "../../include/webserv/loadConfiguration.hpp"
# include "../../include/webserv/http.hpp"
# include <arpa/inet.h>


static void isValidServer(const Context* context,  const Client* client, std::vector<const Context*>& servers) {
    if (context->getName() == SERVER_CONTEXT) {
        const std::string host = context->getDirective("host").front();
        const std::string port = context->getDirective("port").front();

        const struct sockaddr_storage& addr = client->getPeer();
	
		struct addrinfo *res, hints;
		
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res)  == -1) {
            std::cerr << "webserv: warning: getaddrinfo: " << strerror(errno) << std::endl;
            throw 500;
        }
		
        if (res->ai_family == addr.ss_family) {
            if (res->ai_addr->sa_family == AF_INET) {
                const struct sockaddr_in* addr4 = reinterpret_cast<const struct sockaddr_in*>(&addr);
                struct sockaddr_in* serverAddr4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
                if (addr4->sin_port == serverAddr4->sin_port) {
                    if (addr4->sin_addr.s_addr == serverAddr4->sin_addr.s_addr || serverAddr4->sin_addr.s_addr == INADDR_ANY) {
                        servers.push_back(context);
                    }
                }
            } else if (res->ai_family == AF_INET6) {
                const struct sockaddr_in6* addr6 = reinterpret_cast<const struct sockaddr_in6*>(&addr);
                struct sockaddr_in6* serverAddr6 = reinterpret_cast<struct sockaddr_in6*>(res->ai_addr);
                if (addr6->sin6_port == serverAddr6->sin6_port) {
                    if (memcmp(&(addr6->sin6_addr.__u6_addr), &(serverAddr6->sin6_addr), 16) == 0 || memcmp(&(serverAddr6->sin6_addr), &(in6addr_any), 16) == 0) {
						servers.push_back(context);
                    }
                }
            }
        }
    }

    for (std::vector<Context*>::const_iterator it = context->getChildren().begin(); it != context->getChildren().end(); ++it) {
        isValidServer(*it, client, servers);
    }
}

static int __strcmp_(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2)
	{ ++s1; ++s2; }
	return *s1 - *s2;
}

static const Context* __get_match_server_context_(std::vector<const Context*> &servers, const std::string &host)
{
	std::map<const std::string, std::vector<std::string> >::const_iterator d_begin, d_end;
	std::vector<std::string>::const_iterator begin, end;
	
	for (size_t i = 0; i < servers.size(); ++i)
	{
		d_begin = (servers[i])->getDirectives().find(NAME_DIRECTIVE);
		d_end = (servers[i])->getDirectives().end();
		if (d_begin != d_end)
		{
			for (begin = d_begin->second.begin(), end = d_end->second.end(); begin != end; ++begin)
				if (*begin == host)
					return servers[i];
		}
	}
	return servers[0];
}

static const Context* __get_match_location_context_(const std::vector<Context*> &locations, const std::string &path)
{
	int		character;
	std::map<std::string, const Context*, std::greater<std::string> > locate;
	std::map<std::string, const Context*, std::greater<std::string> >::const_iterator begin, end;
	
	if (locations.empty())
		throw "no location found";
	for (size_t i = 0; i < locations.size(); ++i)
		locate.insert(std::make_pair(locations[i]->getArgs()[1], locations[i]));
	
	for (begin = locate.begin(), end = locate.end(); begin != end; ++begin)
	{
		if (begin->first == "/")
			return begin->second;
		character = __strcmp_(path.c_str(), begin->first.c_str());
		if (character == 0)
		{
			// Return 301 move to path/to'/' (slash at the end)
			throw 301;
		}
		if (character == '/')
			return begin->second;
	}
	throw "no match any location: Forbidden";
}

static const Context* __get_location_context_(const Request *request, std::vector<const Context*> &servers)
{
#ifdef DEBUG_GET_LOCATION
	try
	{
#endif
		const Context *server = __get_match_server_context_(servers, request->headers.find("Host")->second);
		const Context *location = __get_match_location_context_(server->getChildren(), request->path);
#ifdef DEBUG_GET_LOCATION
		std::cout << "====================== LOCATION DIRECTIVE ==================\n" ;
		std::cout << "Path: " << request->path << '\n';
		std::cout << "Location: " << location->getArgs()[0] << "   " << location->getArgs()[1] << '\n' ;
		std::map<const std::string, std::vector<std::string> >::const_iterator b, end;
		for (b = location->getDirectives().begin(), end = location->getDirectives().end(); b != end; ++b)
		{
			std::cout << b->first << ": " ;
			for (std::vector<std::string>::const_iterator bg = b->second.begin(), ed = b->second.end(); bg != ed; ++bg)
				std::cout << *bg << "  ";
			std::cout << '\n' ;
		}
#endif
	return location;
#ifdef DEBUG_GET_LOCATION
	}
	catch (const char *error)
	{
		std::cout << "Error: " << error << '\n';
		throw ;
	}
#endif
}

const Context* HTTP::blockMatchAlgorithm(Client* client, const Request* request, const Context* const configuration) {
    std::vector<const Context*> servers;

    for (std::vector<Context*>::const_iterator it = configuration->getChildren().begin(); it != configuration->getChildren().end(); ++it) {
        isValidServer(*it, client, servers);
    }

    return __get_location_context_(request, servers);
}
