class Module {
    public:
      Module(const std::string &name, std::string &port_list,
             std::string &parameter_list, std::string &module_body)
          : name(name), port_list(port_list), parameter_list(parameter_list),
            module_body(module_body) {}
    
      std::string name;
      std::string port_list;
      std::string parameter_list;
      std::string module_body;
};