#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleIpConfiguration");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (3);

  NS_LOG_INFO ("Install Internet Stack.");
  InternetStackHelper stack;
  stack.Install (nodes);

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (nodes);

  NS_LOG_INFO ("Print IP Addresses.");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ipv4Address ipAddr = interfaces.GetAddress (i);
      NS_LOG_INFO ("Node " << node->GetId () << " has IP address " << ipAddr);
    }

  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}