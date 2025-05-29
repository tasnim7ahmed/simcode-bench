#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  uint32_t numNodes = 4;
  NodeContainer nodes;
  nodes.Create(numNodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  std::vector<NetDeviceContainer> devices(numNodes - 1);
  std::vector<Ipv4InterfaceContainer> interfaces(numNodes - 1);

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      devices[i] = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
      interfaces[i] = address.Assign(devices[i]);
    }

  Ptr<Ipv4> ipv4;
  std::cout << "Assigned IP addresses:\n";
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      ipv4 = nodes.Get(i)->GetObject<Ipv4>();
      std::cout << "Node " << i << ":\n";
      for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j)
        {
          uint32_t nAddresses = ipv4->GetNAddresses(j);
          for (uint32_t k = 0; k < nAddresses; ++k)
            {
              Ipv4InterfaceAddress addr = ipv4->GetAddress(j, k);
              if (addr.GetLocal() != Ipv4Address("127.0.0.1"))
                {
                  std::cout << "  Interface " << j << ": "
                            << addr.GetLocal() << "\n";
                }
            }
        }
    }

  Simulator::Stop(Seconds(1.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}