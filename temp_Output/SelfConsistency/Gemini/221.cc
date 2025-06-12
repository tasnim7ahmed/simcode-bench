#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include <iostream>

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

  Address m_peerAddress1;
  Address m_peerAddress2;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Socket *m_socket;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient() : m_socket(nullptr), m_sendEvent(), m_packetsSent(0) {}

UdpClient::~UdpClient() {
  if (m_socket != nullptr) {
    m_socket->Close();
    Simulator::Destroy(); // Correctly destroy the socket
  }
}

void UdpClient::Setup(Address address1, Address address2, uint32_t packetSize,
                      uint32_t numPackets, DataRate dataRate) {
  m_peerAddress1 = address1;
  m_peerAddress2 = address2;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void UdpClient::StartApplication(void) {
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket->Bind() == -1) {
    NS_FATAL_ERROR("Failed to bind socket");
  }
  m_socket->Connect(m_peerAddress1); // Connect to the first server
  m_socket->SetAllowBroadcast(true);
  m_packetsSent = 0;
  SendPacket();
}

void UdpClient::StopApplication(void) {
  Simulator::Cancel(m_sendEvent);
  if (m_socket != nullptr) {
    m_socket->Close();
  }
}

void UdpClient::SendPacket(void) {
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  m_packetsSent++;
  if (m_packetsSent < m_numPackets) {
    Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
    m_sendEvent = Simulator::Schedule(tNext, &UdpClient::SendPacket, this);

    // After sending to server1, connect to server2 and send a packet
    if (m_packetsSent == m_numPackets / 2) {
      m_socket->Close();
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      if (m_socket->Bind() == -1) {
        NS_FATAL_ERROR("Failed to bind socket");
      }
      m_socket->Connect(m_peerAddress2); // Connect to the second server
      m_socket->SetAllowBroadcast(true);
      Ptr<Packet> packet2 = Create<Packet>(m_packetSize);
      m_socket->Send(packet2);
      m_packetsSent++; // Increment packets sent counter
    }
  }
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
  void PrintReceivedData(Ptr<Socket> socket);

  uint16_t m_port;
  Socket *m_socket;
  Address m_localAddress;
};

UdpServer::UdpServer() : m_port(0), m_socket(nullptr) {}

UdpServer::~UdpServer() {
  if (m_socket != nullptr) {
    m_socket->Close();
  }
}

void UdpServer::Setup(uint16_t port) { m_port = port; }

void UdpServer::StartApplication(void) {
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  if (m_socket->Bind(local) == -1) {
    NS_FATAL_ERROR("Failed to bind socket");
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication(void) {
  if (m_socket != nullptr) {
    m_socket->Close();
  }
}

void UdpServer::HandleRead(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from))) {
    uint32_t packetSize = packet->GetSize();
    std::cout << "Server received " << packetSize << " bytes from "
              << InetSocketAddress::ConvertFrom(from).GetIpv4() << " on port "
              << m_port << " at time " << Simulator::Now().GetSeconds()
              << " s" << std::endl;
    PrintReceivedData(socket); // Added to actually process received data.
  }
}

void UdpServer::PrintReceivedData(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> packet;

  while ((packet = socket->RecvFrom(from))) {
    uint8_t *buffer = new uint8_t[packet->GetSize()];
    packet->CopyData(buffer, packet->GetSize());

    std::cout << "Received data: ";
    for (uint32_t i = 0; i < packet->GetSize(); ++i) {
      std::cout << buffer[i];
    }
    std::cout << std::endl;

    delete[] buffer;
  }
}

int main(int argc, char *argv[]) {
  LogComponentEnable("UdpClientServer", LOG_LEVEL_INFO);

  // Define simulation parameters
  uint16_t port1 = 9;
  uint16_t port2 = 10;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  DataRate dataRate("1Mbps");
  Time simulationTime = Seconds(10.0);

  // Create nodes
  NodeContainer clientNode;
  clientNode.Create(1);
  NodeContainer serverNode1;
  serverNode1.Create(1);
  NodeContainer serverNode2;
  serverNode2.Create(1);
  NodeContainer switchNode;
  switchNode.Create(1);

  // Create channels
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(dataRate));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer clientDevices = csma.Install(NodeContainer(clientNode, switchNode));
  NetDeviceContainer server1Devices = csma.Install(NodeContainer(serverNode1, switchNode));
  NetDeviceContainer server2Devices = csma.Install(NodeContainer(serverNode2, switchNode));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(clientNode);
  internet.Install(serverNode1);
  internet.Install(serverNode2);
  internet.Install(switchNode);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer clientInterfaces = ipv4.Assign(clientDevices);
  Ipv4InterfaceContainer server1Interfaces = ipv4.Assign(server1Devices);
  Ipv4InterfaceContainer server2Interfaces = ipv4.Assign(server2Devices);
  ipv4.Assign(NetDeviceContainer(switchNode.GetDevice(0))); // Assign IP to switch
  ipv4.Assign(NetDeviceContainer(switchNode.GetDevice(1))); // Assign IP to switch
  ipv4.Assign(NetDeviceContainer(switchNode.GetDevice(2))); // Assign IP to switch

  Ipv4Address clientAddress = clientInterfaces.GetAddress(0);
  Ipv4Address server1Address = server1Interfaces.GetAddress(0);
  Ipv4Address server2Address = server2Interfaces.GetAddress(0);

  // Create and install UDP client application
  Ptr<UdpClient> clientApp = CreateObject<UdpClient>();
  clientApp->Setup(InetSocketAddress(server1Address, port1),
                   InetSocketAddress(server2Address, port2), packetSize,
                   numPackets, dataRate);
  clientNode.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(simulationTime);

  // Create and install UDP server applications
  Ptr<UdpServer> serverApp1 = CreateObject<UdpServer>();
  serverApp1->Setup(port1);
  serverNode1.Get(0)->AddApplication(serverApp1);
  serverApp1->SetStartTime(Seconds(0.0));
  serverApp1->SetStopTime(simulationTime);

  Ptr<UdpServer> serverApp2 = CreateObject<UdpServer>();
  serverApp2->Setup(port2);
  serverNode2.Get(0)->AddApplication(serverApp2);
  serverApp2->SetStartTime(Seconds(0.0));
  serverApp2->SetStopTime(simulationTime);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Run the simulation
  Simulator::Stop(simulationTime);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}