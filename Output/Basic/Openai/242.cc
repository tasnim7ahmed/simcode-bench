#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/csma-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SixLowpanIotUdpExample");

class PeriodicUdpClient : public Application
{
public:
  PeriodicUdpClient() {}
  virtual ~PeriodicUdpClient() {}

  void Setup(Address address, uint16_t port, uint32_t packetSize, Time interval, uint32_t maxPackets = 0)
  {
    m_peerAddress = address;
    m_peerPort = port;
    m_packetSize = packetSize;
    m_interval = interval;
    m_maxPackets = maxPackets;
    m_sent = 0;
  }

private:
  virtual void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(Inet6SocketAddress(Inet6Address::ConvertFrom(m_peerAddress), m_peerPort));
    SendPacket();
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
    Simulator::Cancel(m_sendEvent);
  }

  void SendPacket()
  {
    if (m_maxPackets && m_sent >= m_maxPackets)
      return;
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    ++m_sent;
    m_sendEvent = Simulator::Schedule(m_interval, &PeriodicUdpClient::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  Time m_interval;
  uint32_t m_maxPackets;
  uint32_t m_sent;
  EventId m_sendEvent;
};

int main(int argc, char *argv[])
{
  uint32_t numDevices = 5;
  uint32_t packetSize = 40;
  double interval = 2.0;
  uint16_t serverPort = 5678;

  CommandLine cmd;
  cmd.AddValue("numDevices", "Number of IoT devices", numDevices);
  cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
  cmd.AddValue("interval", "Inter-packet interval (s)", interval);
  cmd.Parse(argc, argv);

  NodeContainer iotDevices;
  iotDevices.Create(numDevices);
  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer allNodes;
  allNodes.Add(iotDevices);
  allNodes.Add(serverNode);

  // LR-WPAN (IEEE 802.15.4)
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrwpanDevs = lrWpanHelper.Install(allNodes);

  // Assign a PAN ID (arbitrarily 0x1234)
  lrWpanHelper.AssociateToPan(lrwpanDevs, 0x1234);

  // 6LoWPAN over 802.15.4
  SixLowPanHelper sixlowpanHelper;
  NetDeviceContainer sixlowpanDevs = sixlowpanHelper.Install(lrwpanDevs);

  // Install the internet stack
  InternetStackHelper internetv6;
  internetv6.Install(allNodes);

  // IPv6 address assignment
  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(sixlowpanDevs);

  // Remove link-local addresses and set up global addresses
  for (uint32_t i = 0; i < interfaces.GetN(); ++i)
  {
    interfaces.SetForwarding(i, true);
    interfaces.SetDefaultRouteInAllNodes(i);
  }

  // UDP server on the central node
  Ptr<Node> server = serverNode.Get(0);
  UdpServerHelper udpServer(serverPort);
  ApplicationContainer serverApps = udpServer.Install(server);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(60.0));

  // UDP periodic clients on IoT nodes
  for (uint32_t i = 0; i < numDevices; ++i)
  {
    Ptr<Node> clientNode = iotDevices.Get(i);
    Ptr<PeriodicUdpClient> app = CreateObject<PeriodicUdpClient>();
    Ipv6Address peerAddr = interfaces.GetAddress(numDevices, 1); // server's global address
    Address sinkAddress = peerAddr;
    app->Setup(sinkAddress, serverPort, packetSize, Seconds(interval));
    clientNode->AddApplication(app);
    app->SetStartTime(Seconds(1.0 + i * 0.25));
    app->SetStopTime(Seconds(60.0));
  }

  Simulator::Stop(Seconds(61.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}