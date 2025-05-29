#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

class TracePacketReader
{
public:
  explicit TracePacketReader(const std::string &filename)
  {
    std::ifstream infile(filename, std::ios::in);
    if (!infile.is_open()) {
      NS_FATAL_ERROR("Unable to open trace file: " << filename);
    }
    std::string line;
    while (std::getline(infile, line)) {
      std::istringstream iss(line);
      double time;
      uint32_t size;
      if (!(iss >> time >> size)) { continue; }
      m_times.push_back(time);
      m_sizes.push_back(size);
    }
  }

  uint32_t GetNumEvents() const { return m_times.size(); }
  double GetTime(uint32_t idx) const { return m_times[idx]; }
  uint32_t GetSize(uint32_t idx) const { return m_sizes[idx]; }

private:
  std::vector<double> m_times;
  std::vector<uint32_t> m_sizes;
};

void SendPacket(Ptr<Socket> socket, Address peer, uint32_t size)
{
  Ptr<Packet> packet = Create<Packet>(size);
  socket->SendTo(packet, 0, peer);
}

int main(int argc, char *argv[])
{
  std::string traceFile = "trace.txt";
  bool useIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd;
  cmd.AddValue("traceFile", "Trace file for packet times/sizes (columns: time[s] size[bytes])", traceFile);
  cmd.AddValue("useIpv6", "Enable IPv6 addressing (default: false = IPv4)", useIpv6);
  cmd.AddValue("enableLogging", "Enable logging (INFO level)", enableLogging);
  cmd.Parse(argc, argv);

  if (enableLogging) {
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4Address serverIpv4("10.1.1.2");
  Ipv4Address clientIpv4("10.1.1.1");
  Ipv6Address serverIpv6("2001:1::2");
  Ipv6Address clientIpv6("2001:1::1");

  Ipv4InterfaceContainer ipv4Ifs;
  Ipv6InterfaceContainer ipv6Ifs;

  if (!useIpv6) {
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4Ifs = ipv4.Assign(devices);
  } else {
    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:1::", 64);
    ipv6Ifs = ipv6.Assign(devices);
    ipv6Ifs.SetForwarding(0, true);
    ipv6Ifs.SetForwarding(1, true);
    ipv6Ifs.SetDefaultRouteInAllNodes(0);
    ipv6Ifs.SetDefaultRouteInAllNodes(1);
  }

  uint16_t port = 9999;

  UdpServerHelper udpServer(port);

  ApplicationContainer serverApp;
  if (!useIpv6) {
    serverApp = udpServer.Install(nodes.Get(1));
  } else {
    serverApp = udpServer.Install(nodes.Get(1));
  }
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  TracePacketReader trace(traceFile);

  Ptr<Socket> srcSocket;
  Address destAddr;
  if (!useIpv6) {
    srcSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    destAddr = InetSocketAddress(ipv4Ifs.GetAddress(1), port);
    srcSocket->Connect(destAddr);
  } else {
    srcSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    destAddr = Inet6SocketAddress(ipv6Ifs.GetAddress(1, 1), port);
    srcSocket->Connect(destAddr);
  }

  Ptr<Socket> sendSocket = srcSocket;
  for (uint32_t i = 0; i < trace.GetNumEvents(); ++i) {
    double sendTime = trace.GetTime(i) + 2.0;
    if (sendTime > 10.0) break;
    Simulator::Schedule(Seconds(sendTime),
                        &SendPacket, sendSocket, destAddr, trace.GetSize(i));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}