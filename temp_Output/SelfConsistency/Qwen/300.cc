#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigBeeSensorNetwork");

class ZigBeePacketSink : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    return TypeId("ZigBeePacketSink")
        .SetParent<Application>()
        .AddConstructor<ZigBeePacketSink>();
  }

  ZigBeePacketSink() {}
  virtual ~ZigBeePacketSink() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&ZigBeePacketSink::Receive, this));
  }

  virtual void StopApplication(void) {}

  void Receive(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("ZigBeeSensorNetwork", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(10);

  Ptr<Node> coordinator = nodes.Get(0);
  NodeContainer endDevices = NodeContainer(nodes.Begin() + 1, nodes.End());

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(20.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  ZigbeeHelper zigbee;
  zigbee.SetDeviceType(ZigbeeDevice::DEVICE_TYPE_COORDINATOR);
  zigbee.Install(coordinator);

  zigbee.SetDeviceType(ZigbeeDevice::DEVICE_TYPE_END_DEVICE);
  zigbee.Install(endDevices);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces;
  interfaces = zigbee.AssignIpv4Addresses(address);

  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    if (nodes.Get(i) == coordinator)
    {
      continue;
    }

    UdpClientServerHelper clientServerHelper(interfaces.GetAddress(0), 9);
    clientServerHelper.SetAttribute("MaxPackets", UintegerValue(10000));
    clientServerHelper.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    clientServerHelper.SetAttribute("PacketSize", UintegerValue(128));

    ApplicationContainer clientApp = clientServerHelper.Install(nodes.Get(i));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(10.0));
  }

  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), 9));
  ApplicationContainer sinkApp = sinkHelper.Install(coordinator);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  zigbee.EnablePcapAll("zigbee_sensor_network");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}