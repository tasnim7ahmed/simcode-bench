#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(5);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesA_B = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesA_C = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devicesA_D = p2p.Install(nodes.Get(0), nodes.Get(3));
    NetDeviceContainer devicesA_E = p2p.Install(nodes.Get(0), nodes.Get(4));

    Ipv4InterfaceContainer interfaces = address.Assign(devicesA_B);
    interfaces.Add(address.Assign(devicesA_C));
    interfaces.Add(address.Assign(devicesA_D));
    interfaces.Add(address.Assign(devicesA_E));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    uint16_t port = 9;
    Ipv4Address multicastGroup("225.1.2.3");

    PacketSinkHelper sinkHelper(tid, InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkAppB = sinkHelper.Install(nodes.Get(1));
    ApplicationContainer sinkAppC = sinkHelper.Install(nodes.Get(2));
    ApplicationContainer sinkAppD = sinkHelper.Install(nodes.Get(3));
    ApplicationContainer sinkAppE = sinkHelper.Install(nodes.Get(4));
    sinkAppB.Start(Seconds(1.0));
    sinkAppB.Stop(Seconds(10.0));
    sinkAppC.Start(Seconds(1.0));
    sinkAppC.Stop(Seconds(10.0));
    sinkAppD.Start(Seconds(1.0));
    sinkAppD.Stop(Seconds(10.0));
    sinkAppE.Start(Seconds(1.0));
    sinkAppE.Stop(Seconds(10.0));

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    onOffHelper.SetConstantRate(DataRate("1Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app = onOffHelper.Install(nodes.Get(0));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(10.0));

    for (int i = 1; i < 5; ++i) {
      Ptr<Node> node = nodes.Get(i);
      for (uint32_t j = 0; j < node->GetNNodes(); ++j) {
          Ptr<NetDevice> dev = node->GetDevice(j);
          Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
          if (ipv4 != nullptr) {
              int32_t interface = ipv4->GetInterfaceForDevice(dev);
              if (interface != -1) {
                  ipv4->SetRoutingProtocolForInterface(interface, Ipv4RoutingProtocolType::PROACTIVE);
              }
          }
      }
    }

    Simulator::Stop(Seconds(10.0));

    p2p.EnablePcapAll("multicast");

    Simulator::Run();

    Simulator::Destroy();

    return 0;
}