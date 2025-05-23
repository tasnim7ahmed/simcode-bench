#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h" // For fixed position

#include <iostream>
#include <string>

namespace ns3 {

/**
 * \brief A custom UDP receiver application that logs received packets.
 *
 * This application creates a UDP socket and binds it to a specified port.
 * When a packet is received, it logs the time, node ID, packet size,
 * source IP address, and port to standard output.
 */
class CustomUdpReceiver : public Application
{
public:
  static TypeId GetTypeId(void);
  CustomUdpReceiver ();
  virtual ~CustomUdpReceiver ();

protected:
  virtual void DoInitialize(void);
  virtual void DoDispose(void);
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  /**
   * \brief Callback for handling received UDP packets.
   * \param socket The UDP socket that received the packet.
   */
  void HandleRead (Ptr<Socket> socket);

  uint16_t m_port;      ///< Port number to listen on
  Ptr<Socket> m_socket; ///< The UDP socket
};

NS_OBJECT_ENSURE_REGISTERED (CustomUdpReceiver);

TypeId CustomUdpReceiver::GetTypeId(void)
{
  static TypeId tid = TypeId ("CustomUdpReceiver")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<CustomUdpReceiver> ()
    .AddAttribute ("Port",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (9), // Default to discard port
                   MakeUintegerAccessor (&CustomUdpReceiver::m_port),
                   MakeUintegerChecker<uint16_t> ())
    ;
  return tid;
}

CustomUdpReceiver::CustomUdpReceiver () : m_port (0)
{
}

CustomUdpReceiver::~CustomUdpReceiver ()
{
  m_socket = 0;
}

void CustomUdpReceiver::DoInitialize(void)
{
  Application::DoInitialize();
}

void CustomUdpReceiver::DoDispose(void)
{
  Application::DoDispose();
}

void CustomUdpReceiver::StartApplication(void)
{
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      if (m_socket->Bind (local) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
    }

  // Set the receive callback for the socket
  m_socket->SetRecvCallback (MakeCallback (&CustomUdpReceiver::HandleRead, this));
}

void CustomUdpReceiver::StopApplication(void)
{
  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
    }
}

void CustomUdpReceiver::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::Is<InetSocketAddress> (from))
        {
          InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
          // Log when a packet is received by a receiver node
          std::cout << "Time: " << Simulator::Now ().GetSeconds () << "s, "
                    << "Node: " << GetNode()->GetId() << ", "
                    << "Received: " << packet->GetSize () << " bytes from "
                    << addr.GetIpv4 () << ":" << addr.GetPort() << std::endl;
        }
    }
}

} // namespace ns3

int main (int argc, char *argv[])
{
  using namespace ns3;

  // Simulation Parameters
  uint32_t numReceivers = 3;       // Number of receiver nodes
  uint32_t payloadSize = 1000;     // Size of UDP payload in bytes
  double packetInterval = 1.0;     // Time interval between packets (seconds)
  double simulationDuration = 10.0; // Total duration of the simulation (seconds)
  uint16_t broadcastPort = 9;      // UDP port for broadcast (discard port)
  uint32_t senderNodeId = 0;       // The node at index 0 will be the sender

  // Command line argument parsing
  CommandLine cmd (__FILE__);
  cmd.AddValue ("numReceivers", "Number of receiver nodes", numReceivers);
  cmd.AddValue ("payloadSize", "Size of UDP payload in bytes", payloadSize);
  cmd.AddValue ("packetInterval", "Time interval between packets (seconds)", packetInterval);
  cmd.AddValue ("simulationDuration", "Total duration of the simulation (seconds)", simulationDuration);
  cmd.Parse (argc, argv);

  // Set time resolution
  Time::SetResolution (Time::NS);

  // 1. Create Nodes
  NodeContainer allNodes;
  allNodes.Create (1 + numReceivers); // One sender node + multiple receiver nodes

  Ptr<Node> senderNode = allNodes.Get (senderNodeId);
  NodeContainer receiverNodes;
  for (uint32_t i = 0; i < numReceivers; ++i)
  {
    receiverNodes.Add (allNodes.Get (1 + i)); // Receivers start from index 1
  }

  // 2. Configure Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g); // Use 802.11g standard

  YansWifiPhyHelper phy;
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO); // For PCAP tracing
  
  YansWifiChannelHelper channel;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel"); // Simple propagation model
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac"); // Set to Ad-hoc mode for direct communication

  NetDeviceContainer devices = wifi.Install (phy, mac, allNodes);

  // 3. Install Internet Stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  // 4. Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0"); // A /24 network
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Get the broadcast address for the subnet
  Ipv4Address broadcastAddress = interfaces.GetAddress (senderNodeId).GetSubnetBroadcastAddress ();

  // 5. Install Applications

  // Receiver Applications (CustomUdpReceiver)
  ApplicationContainer receiverApps;
  for (uint32_t i = 0; i < numReceivers; ++i)
  {
    Ptr<CustomUdpReceiver> receiver = CreateObject<CustomUdpReceiver> ();
    receiver->SetAttribute ("Port", UintegerValue (broadcastPort));
    receiverNodes.Get (i)->AddApplication (receiver);
    receiverApps.Add (receiver);
  }
  receiverApps.Start (Seconds (0.0)); // Start receivers at the beginning of simulation
  receiverApps.Stop (Seconds (simulationDuration));

  // Sender Application (OnOffApplication)
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (broadcastAddress, broadcastPort));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]")); // Sender is always "On"
  onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  
  // Calculate DataRate based on packet size and interval: (payloadSize * 8 bits/byte) / interval seconds
  DataRate calculatedRate = DataRate((uint64_t)payloadSize * 8 / packetInterval);
  onoff.SetAttribute ("DataRate", DataRateValue (calculatedRate));

  ApplicationContainer senderApp = onoff.Install (senderNode);
  senderApp.Start (Seconds (1.0)); // Start sending after 1 second
  senderApp.Stop (Seconds (simulationDuration));

  // 6. Mobility (Static positions for all nodes)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);
  // Set explicit positions (optional, but good for visualization/debugging)
  for (uint32_t i = 0; i < allNodes.GetN (); ++i)
  {
      Ptr<ConstantPositionMobilityModel> position = allNodes.Get(i)->GetObject<ConstantPositionMobilityModel>();
      position->SetPosition(Vector(i * 10.0, 0.0, 0.0)); // Spread nodes along X-axis
  }

  // 7. Enable PCAP tracing for Wi-Fi devices
  phy.EnablePcap ("broadcast-wifi", devices);

  // 8. Run Simulation
  Simulator::Stop (Seconds (simulationDuration + 1.0)); // Stop slightly after applications finish
  Simulator::Run ();
  Simulator::Destroy (); // Clean up memory

  return 0;
}