#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveSimulation");

class V2VWaveExample
{
public:
  V2VWaveExample();
  void Run();

private:
  NodeContainer m_nodes;
  uint32_t m_packetsSent;
  uint32_t m_packetsReceived;

  void SendPacket(Ptr<Socket> socket, uint32_t packetSize, uint32_t numPackets, Time interval);
  void ReceivePacket(Ptr<Socket> socket);
};

V2VWaveExample::V2VWaveExample()
    : m_packetsSent(0), m_packetsReceived(0)
{
}

void V2VWaveExample::Run()
{
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  m_nodes.Create(2);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
  WaveDeviceHelper waveDevHelper = WaveDeviceHelper::Default();
  waveDevHelper.SetMac(mac);
  waveDevHelper.SetPhy(phy);

  NetDeviceContainer devices = waveDevHelper.Install(m_nodes);

  InternetStackHelper internet;
  internet.Install(m_nodes);

  Ipv4AddressHelper ipv4Addr;
  ipv4Addr.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4Addr.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(m_nodes);

  for (uint32_t i = 0; i < m_nodes.GetN(); ++i)
  {
    Ptr<ConstantVelocityMobilityModel> model = m_nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    model->SetVelocity(Vector(20, 0, 0));
  }

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> recvSocket = Socket::CreateSocket(m_nodes.Get(1), tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
  recvSocket->Bind(local);
  recvSocket->SetRecvCallback(MakeCallback(&V2VWaveExample::ReceivePacket, this));

  Ptr<Socket> sendSocket = Socket::CreateSocket(m_nodes.Get(0), tid);
  InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), 80);
  sendSocket->Connect(remote);

  Simulator::Schedule(Seconds(1.0), &V2VWaveExample::SendPacket, this, sendSocket, 1024, 10, Seconds(1.0));

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "Sent packets: " << m_packetsSent << ", Received packets: " << m_packetsReceived << std::endl;
}

void V2VWaveExample::SendPacket(Ptr<Socket> socket, uint32_t packetSize, uint32_t numPackets, Time interval)
{
  if (numPackets > 0)
  {
    Ptr<Packet> packet = Create<Packet>(packetSize);
    socket->Send(packet);
    m_packetsSent++;
    Simulator::Schedule(interval, &V2VWaveExample::SendPacket, this, socket, packetSize, numPackets - 1, interval);
  }
}

void V2VWaveExample::ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    m_packetsReceived++;
    NS_LOG_UNCOND("Received packet of size: " << packet->GetSize());
  }
}

int main(int argc, char *argv[])
{
  LogComponentEnable("V2VWaveSimulation", LOG_LEVEL_INFO);
  V2VWaveExample simulation;
  simulation.Run();
  return 0;
}