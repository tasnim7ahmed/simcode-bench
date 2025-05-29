#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodPlusSimulation");

std::ofstream CongestionWindowTraceFile;
std::ofstream SlowStartThresholdTraceFile;
std::ofstream RttTraceFile;
std::ofstream RtoTraceFile;
std::ofstream InFlightBytesTraceFile;

std::map<Ptr<TcpSocket>, std::stringstream> CongestionWindowTrace;
std::map<Ptr<TcpSocket>, std::stringstream> SlowStartThresholdTrace;
std::map<Ptr<TcpSocket>, std::stringstream> RttTrace;
std::map<Ptr<TcpSocket>, std::stringstream> RtoTrace;
std::map<Ptr<TcpSocket>, std::stringstream> InFlightBytesTrace;

static void
CwndTracer(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string socketStr;
  std::string socketId = context;
  while ((pos = socketStr.find(delimiter)) != std::string::npos) {
      token = socketStr.substr(0, pos);
      socketStr.erase(0, pos + delimiter.length());
  }
  socketStr = socketId;
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);

  Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(GetObject<Socket>(socketStr));

  if (CongestionWindowTrace.find(socket) == CongestionWindowTrace.end()) {
        CongestionWindowTrace[socket] << "Time\tCwnd" << std::endl;
    }
    CongestionWindowTrace[socket] << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

static void
SsthTracer(std::string context, uint32_t oldSsth, uint32_t newSsth)
{
    std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string socketStr;
  std::string socketId = context;
  while ((pos = socketStr.find(delimiter)) != std::string::npos) {
      token = socketStr.substr(0, pos);
      socketStr.erase(0, pos + delimiter.length());
  }
  socketStr = socketId;
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);

  Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(GetObject<Socket>(socketStr));

  if (SlowStartThresholdTrace.find(socket) == SlowStartThresholdTrace.end()) {
        SlowStartThresholdTrace[socket] << "Time\tSsth" << std::endl;
    }
    SlowStartThresholdTrace[socket] << Simulator::Now().GetSeconds() << "\t" << newSsth << std::endl;
}

static void
RttTracer(std::string context, Time oldRtt, Time newRtt)
{
    std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string socketStr;
  std::string socketId = context;
  while ((pos = socketStr.find(delimiter)) != std::string::npos) {
      token = socketStr.substr(0, pos);
      socketStr.erase(0, pos + delimiter.length());
  }
  socketStr = socketId;
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);

  Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(GetObject<Socket>(socketStr));
  if (RttTrace.find(socket) == RttTrace.end()) {
        RttTrace[socket] << "Time\tRtt" << std::endl;
    }
    RttTrace[socket] << Simulator::Now().GetSeconds() << "\t" << newRtt.GetSeconds() << std::endl;
}

static void
RtoTracer(std::string context, Time oldRto, Time newRto)
{
   std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string socketStr;
  std::string socketId = context;
  while ((pos = socketStr.find(delimiter)) != std::string::npos) {
      token = socketStr.substr(0, pos);
      socketStr.erase(0, pos + delimiter.length());
  }
  socketStr = socketId;
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);

  Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(GetObject<Socket>(socketStr));
  if (RtoTrace.find(socket) == RtoTrace.end()) {
        RtoTrace[socket] << "Time\tRto" << std::endl;
    }
    RtoTrace[socket] << Simulator::Now().GetSeconds() << "\t" << newRto.GetSeconds() << std::endl;
}

static void
InFlightBytesTracer(std::string context, uint32_t oldInFlight, uint32_t newInFlight)
{
   std::string delimiter = "/";
  size_t pos = 0;
  std::string token;
  std::string socketStr;
  std::string socketId = context;
  while ((pos = socketStr.find(delimiter)) != std::string::npos) {
      token = socketStr.substr(0, pos);
      socketStr.erase(0, pos + delimiter.length());
  }
  socketStr = socketId;
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);
  pos = socketId.find("/");
  socketStr.erase(0, pos+1);

  Ptr<TcpSocket> socket = DynamicCast<TcpSocket>(GetObject<Socket>(socketStr));
  if (InFlightBytesTrace.find(socket) == InFlightBytesTrace.end()) {
        InFlightBytesTrace[socket] << "Time\tInFlightBytes" << std::endl;
    }
    InFlightBytesTrace[socket] << Simulator::Now().GetSeconds() << "\t" << newInFlight << std::endl;
}

int
main(int argc, char* argv[])
{
  bool tracing = false;
  std::string bandwidth = "5Mbps";
  std::string delay = "2ms";
  double errorRate = 0.00001;
  std::string tcpVariant = "TcpWestwoodPlus";
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable or disable tracing.", tracing);
  cmd.AddValue("bandwidth", "The bottleneck bandwidth.", bandwidth);
  cmd.AddValue("delay", "The bottleneck link delay.", delay);
  cmd.AddValue("errorRate", "The bottleneck link error rate.", errorRate);
  cmd.AddValue("tcpVariant", "Transport protocol to use: TcpWestwoodPlus, TcpHybla, "
                 "TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpCubic, TcpIllinois, TcpYeah, TcpBbr.", tcpVariant);
  cmd.AddValue("simulationTime", "Simulation time in seconds.", simulationTime);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpVariant));

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));
  pointToPoint.SetQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("1000p"));

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  pointToPoint.SetErrorModel(em);

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1400));
  source.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(simulationTime));

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime));

  if (tracing) {
        AsciiTraceHelper ascii;
        pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-westwood-plus.tr"));

        CongestionWindowTraceFile.open("cwnd.dat");
        SlowStartThresholdTraceFile.open("ssth.dat");
        RttTraceFile.open("rtt.dat");
        RtoTraceFile.open("rto.dat");
        InFlightBytesTraceFile.open("inflight.dat");

        for (uint32_t i = 0; i < nodes.Get(0)->GetNApplications(); ++i) {
            Ptr<Application> app = nodes.Get(0)->GetApplication(i);
            if (app->GetInstanceTypeId() == BulkSendApplication::GetTypeId()) {
                Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(app);
                Ptr<Socket> socket = bulkSendApp->GetSocket();
                if (socket != NULL) {
                    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/"
                                                "CongestionWindow", MakeCallback(&CwndTracer));
                    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/"
                                                "SlowStartThreshold", MakeCallback(&SsthTracer));
                    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/"
                                                "RTT", MakeCallback(&RttTracer));
                    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/"
                                                "RTO", MakeCallback(&RtoTracer));
                    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/"
                                                "BytesInFlight", MakeCallback(&InFlightBytesTracer));
                }
            }
        }
    }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

    if(tracing) {
        for (auto const& [socket, trace] : CongestionWindowTrace) {
            CongestionWindowTraceFile << trace.str();
        }
        for (auto const& [socket, trace] : SlowStartThresholdTrace) {
            SlowStartThresholdTraceFile << trace.str();
        }
        for (auto const& [socket, trace] : RttTrace) {
            RttTraceFile << trace.str();
        }
        for (auto const& [socket, trace] : RtoTrace) {
            RtoTraceFile << trace.str();
        }
        for (auto const& [socket, trace] : InFlightBytesTrace) {
            InFlightBytesTraceFile << trace.str();
        }

        CongestionWindowTraceFile.close();
        SlowStartThresholdTraceFile.close();
        RttTraceFile.close();
        RtoTraceFile.close();
        InFlightBytesTraceFile.close();
    }
  Simulator::Destroy();
  return 0;
}