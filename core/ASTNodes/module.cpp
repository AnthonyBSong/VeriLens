#include "string"
#include "vector"

class Module {
    public:
      Module(const std::string &name, std::vector<std::string> &port_list,
             std::vector<std::string> &parameter_list)
          : name(name), port_list(port_list), parameter_list(parameter_list) {}
      
      std::string name;
      std::vector<std::string> port_list;
      std::vector<std::string> parameter_list;
};