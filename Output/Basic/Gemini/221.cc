#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpClientServer");

class UdpClient : public Application {
public:
  UdpClient();
  virtual ~UdpClient();

  void Setup(Address address1, Address address2, uint32_t packetSize,
             uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);
  void ScheduleTx(void);

  Address m_remoteAddress1;
  Address m_remoteAddress2;
  Ptr<Socket> m_socket1;
  Ptr<Socket> m_socket2;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient()
    : m_socket1(nullptr), m_socket2(nullptr), m_packetSize(0),
      m_numPackets(0), m_dataRate(0), m_sendEvent(), m_running(false),
      m_packetsSent(0) {}

UdpClient::~UdpClient() {
  if (m_socket1) {
    m_socket1->Close();
    m_socket1 = nullptr;
  }
  if (m_socket2) {
    m_socket2->Close();
    m_socket2 = nullptr;
  }
}

void UdpClient::Setup(Address address1, Address address2, uint32_t packetSize,
                      uint32_t numPackets, DataRate dataRate) {
  m_remoteAddress1 = address1;
  m_remoteAddress2 = address2;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void UdpClient::StartApplication(void) {
  m_running = true;
  m_packetsSent = 0;

  m_socket1 = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket1->Bind();
  m_socket1->Connect(m_remoteAddress1);

  m_socket2 = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket2->Bind();
  m_socket2->Connect(m_remoteAddress2);

  ScheduleTx();
}

void UdpClient::StopApplication(void) {
  m_running = false;
  if (m_sendEvent.IsRunning()) {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket1) {
    m_socket1->Close();
  }
  if (m_socket2) {
    m_socket2->Close();
  }
}

void UdpClient::SendPacket(void) {
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket1->Send(packet);
  m_packetsSent++;

  packet = Create<Packet>(m_packetSize);
  m_socket2->Send(packet);
  m_packetsSent++;

  if (m_packetsSent < m_numPackets && m_running) {
    ScheduleTx();
  }
}

void UdpClient::ScheduleTx(void) {
  Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
  m_sendEvent = Simulator::Schedule(tNext, &UdpClient::SendPacket, this);
}

class UdpServer : public Application {
public:
  UdpServer();
  virtual ~UdpServer();

  void Setup(uint16_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);
  void ReceivePacket(Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_running;
};

UdpServer::UdpServer() : m_port(0), m_socket(nullptr), m_running(false) {}

UdpServer::~UdpServer() {
  if (m_socket) {
    m_socket->Close();
  }
}

void UdpServer::Setup(uint16_t port) { m_port = port; }

void UdpServer::StartApplication(void) {
  m_running = true;

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket(GetNode(), tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication(void) {
  m_running = false;
  if (m_socket) {
    m_socket->Close();
  }
}

void UdpServer::HandleRead(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from))) {
    ReceivePacket(socket);
  }
}

void UdpServer::ReceivePacket(Ptr<Socket> socket) {
  NS_LOG_INFO("Server received one packet!");
}

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t nCsma = 3;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  DataRate dataRate("1Mbps");
  uint16_t port1 = 9;
  uint16_t port2 = 10;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("nCsma", "Number of \"CSMA devices/nodes\"", nCsma);
  cmd.AddValue("packetSize", "Size of echo packet", packetSize);
  cmd.AddValue("numPackets", "Number of packets client will send", numPackets);
  cmd.AddValue("dataRate", "Data rate of client", dataRate);
  cmd.AddValue("port1", "Port number for server 1", port1);
  cmd.AddValue("port2", "Port number for server 2", port2);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClientServer", LOG_LEVEL_INFO);
  }

  NodeContainer p2pNodes;
  p2pNodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install(p2pNodes);

  NodeContainer csmaNodes;
  csmaNodes.Add(p2pNodes.Get(0));
  csmaNodes.Create(nCsma - 1);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install(csmaNodes);

  InternetStackHelper internet;
  internet.Install(p2pNodes.Get(0));
  internet.Install(csmaNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = ipv4.Assign(p2pDevices);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = ipv4.Assign(csmaDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ptr<Node> clientNode = p2pNodes.Get(1);
  Ptr<Node> serverNode1 = csmaNodes.Get(1);
  Ptr<Node> serverNode2 = csmaNodes.Get(2);

  UdpServerHelper echoServerHelper1(port1);
  ApplicationContainer serverApps1 = echoServerHelper1.Install(serverNode1);
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(10.0));

  UdpServerHelper echoServerHelper2(port2);
  ApplicationContainer serverApps2 = echoServerHelper2.Install(serverNode2);
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  Ptr<UdpClient> clientApp = CreateObject<UdpClient>();
  clientApp->Setup(InetSocketAddress(csmaInterfaces.GetAddress(1), port1),
                   InetSocketAddress(csmaInterfaces.GetAddress(2), port2),
                   packetSize, numPackets, dataRate);
  clientNode->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(2.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}