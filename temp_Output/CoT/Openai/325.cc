#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

Ptr<Socket> clientSocket;
Ptr<TcpSocketBase> tcpSocketBase;
EventId rttEvent;
Time simStop = Seconds(10.0);

void
LogRtt()
{
  if (tcpSocketBase)
    {
      Time rtt = tcpSocketBase->GetRTT();
      std::cout << Simulator::Now().GetSeconds() << "s: RTT = " << rtt.GetMilliSeconds() << " ms" << std::endl;
    }
  if (Simulator::Now() < simStop)
    {
      rttEvent = Simulator::Schedule(MilliSeconds(100), &LogRtt);
    }
}

void
SocketTracer(Ptr<Socket> socket)
{
  clientSocket = socket;
  tcpSocketBase = DynamicCast<TcpSocketBase>(socket);
  LogRtt();
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t serverPort = 50000;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), serverPort));

  // Server
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), serverPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(simStop);

  // Client
  OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
  clientHelper.SetAttribute("StopTime", TimeValue(simStop));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));

  // Install the socket tracer after the application start
  Simulator::Schedule(Seconds(2.001), []() {
     Ptr<Node> node = NodeList::GetNode(0);
     Ptr<Socket> foundSocket;
     for (uint32_t i = 0; i < node->GetNApplications(); ++i)
       {
         Ptr<Application> app = node->GetApplication(i);
         Ptr<OnOffApplication> onoff = DynamicCast<OnOffApplication>(app);
         if (onoff)
           {
             foundSocket = onoff->GetSocket();
             break;
           }
       }
     if (foundSocket)
       {
         SocketTracer(foundSocket);
       }
  });

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(simStop);
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}