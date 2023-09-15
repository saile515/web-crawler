#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <fmt/core.h>
#include <httplib.h>
#include <iostream>
#include <pqxx/pqxx>
#include <regex>
#include <string>
#include <vector>

static std::string read_webpage(std::string domain,
                                std::vector<std::string> &stack,
                                std::string connection_string) {
    pqxx::connection connection{connection_string};
    pqxx::work transaction{connection};
    bool exists = transaction.query_value<bool>(fmt::format(
        "SELECT EXISTS(SELECT 1 FROM domain WHERE domain='{}')", domain));
    if (exists == false) {
        stack.push_back(domain);
        transaction.exec0(
            fmt::format("INSERT INTO domain VALUES ('{}')", domain));
        transaction.commit();
        httplib::Client client(fmt::format("https://{}", domain));
        auto res = client.Get("/");

        return res->body;
    } else {
        return "";
    }
}
static std::vector<std::string> extract_domains(std::string page) {
    std::vector<std::string> result;
    std::regex exp("href=[\'\"]([^\'\"]*)");
    std::smatch match;

    std::string::const_iterator search_start(page.cbegin());
    while (std::regex_search(search_start, page.cend(), match, exp)) {
        std::string url = match[1];
        {
            std::regex exp("https?://([a-zA-Z0-9-.]+)");
            std::smatch match;
            if (std::regex_search(url.cbegin(), url.cend(), match, exp)) {
                result.push_back(match[1]);
            }
        }

        search_start = match.suffix().first;
    }
    return result;
}

static void add_domain_to_stack(std::string domain,
                                std::vector<std::string> &stack,
                                std::string connection_string) {
    pqxx::connection connection{connection_string};
    pqxx::work transaction{connection};
    bool exists = transaction.query_value<bool>(fmt::format(
        "SELECT EXISTS(SELECT 1 FROM domain WHERE domain='{}')", domain));
    if (exists == false) {
        stack.push_back(domain);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fmt::print("Error: wrong usage, correct usage: web-crawler <origin> "
                   "<connection_string>");
    }

    std::string origin = argv[1];
    std::vector<std::string> stack;
    stack.push_back(origin);

    while (stack.size() > 0) {
        std::cout << fmt::format("Processing: {}, Left in stack: {}\n",
                                 stack[0], stack.size())
                  << std::flush;
        try {
            std::string page = read_webpage(stack[0], stack, argv[2]);
            std::vector<std::string> domains = extract_domains(page);
            for (std::string &domain : domains) {
                add_domain_to_stack(domain, stack, argv[2]);
            }
        } catch (...) {
            std::cout << "Failed to process domain\n" << std::flush;
        }
        stack.erase(stack.begin());
    }
}
