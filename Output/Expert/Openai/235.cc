#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpSink : public Application
{
public:
  UdpSink() {}
  virtual ~UdpSink() {}
  void Setup(Address address, uint16_t port)
  {
    m_local = InetSocketAddress(Ipv4Address::GetAny(), port);
  }
private:
  virtual void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(m_local);
    m_socket->SetRecvCallback(MakeCallback(&UdpSink::HandleRead, this));
  }

  virtual void StopApplication() override
  {
    if (m_socket)
      m_socket->Close();
    m_socket = nullptr;
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_UNCOND("Sink received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
  }
  Ptr<Socket> m_socket;
  Address m_local;
};

int main(int argc, char* argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer sensor, sink;
  sensor.Create(1);
  sink.Create(1);
  NodeContainer endDevices;
  endDevices.Add(sensor);
  endDevices.Add(sink);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0,0.0,0.0));
  positionAlloc->Add(Vector(10.0,0.0,0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(endDevices);

  Ptr<Channel> channel = CreateObject<SimpleChannel>();
  LorawanHelper lorawan;
  lorawan.SetChannel(channel);

  // Create and set up the LoRaWAN gateway (minimal; single cell/base station)
  NodeContainer gateways;
  gateways.Create(1);
  MobilityHelper gwMobility;
  gwMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  Ptr<ListPositionAllocator> gwAlloc = CreateObject<ListPositionAllocator>();
  gwAlloc->Add(Vector(5.0,5.0,10.0));
  gwMobility.SetPositionAllocator(gwAlloc);
  gwMobility.Install(gateways);

  lorawan.InstallGwDevice(gateways);
  lorawan.InstallEndDevice(sensor);
  lorawan.InstallEndDevice(sink);

  // Set up the LoraPhy for better determinism (optional but precise config)
  lorawan.GetPhy(sensor.Get(0))->SetChannel(channel);
  lorawan.GetPhy(sink.Get(0))->SetChannel(channel);
  lorawan.GetPhy(gateways.Get(0))->SetChannel(channel);

  // Internet stack for IP/UDP apps
  InternetStackHelper internet;
  internet.Install(sensor);
  internet.Install(sink);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.0.0", "255.255.255.0");
  NetDeviceContainer edDevices;
  edDevices.Add(lorawan.GetNetDevice(sensor.Get(0)));
  edDevices.Add(lorawan.GetNetDevice(sink.Get(0)));
  edDevices.Add(lorawan.GetNetDevice(gateways.Get(0)));
  Ipv4InterfaceContainer interfaces = ipv4.Assign(edDevices);

  uint16_t port = 4000;
  Ptr<UdpSink> udpSinkApp = CreateObject<UdpSink>();
  udpSinkApp->Setup(interfaces.GetAddress(1), port);
  sink.Get(0)->AddApplication(udpSinkApp);
  udpSinkApp->SetStartTime(Seconds(0.0));
  udpSinkApp->SetStopTime(Seconds(30.0));

  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
  onoff.SetAttribute("DataRate", StringValue("1kbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(30));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(30.0)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=4]"));
  ApplicationContainer apps = onoff.Install(sensor.Get(0));

  Simulator::Stop(Seconds(31.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}