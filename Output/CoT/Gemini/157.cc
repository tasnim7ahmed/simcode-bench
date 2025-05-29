#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <fstream>
#include <iostream>

using namespace ns3;

std::ofstream outputFile;

void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  Time arrivalTime = Simulator::Now ();
  Ipv4Address sourceAddress = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
  Ipv4Address destAddress = socket->GetIpv4Address ();

  outputFile << sourceAddress << ","
             << destAddress << ","
             << packet->GetSize () << ","
             << packet->GetUid () << ","
             << arrivalTime.GetSeconds () << std::endl;
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices[1] = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  devices[2] = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  devices[3] = pointToPoint.Install (nodes.Get (3), nodes.Get (0));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = address.Assign (devices[0]);
  address.NewNetwork ();
  interfaces[1] = address.Assign (devices[1]);
  address.NewNetwork ();
  interfaces[2] = address.Assign (devices[2]);
  address.NewNetwork ();
  interfaces[3] = address.Assign (devices[3]);

  UdpEchoClientHelper echoClient (interfaces[1].GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient2 (interfaces[2].GetAddress (1), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (1.1));
  clientApps2.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient3 (interfaces[3].GetAddress (1), 9);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps3 = echoClient3.Install (nodes.Get (2));
  clientApps3.Start (Seconds (1.2));
  clientApps3.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient4 (interfaces[0].GetAddress (1), 9);
  echoClient4.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient4.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient4.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps4 = echoClient4.Install (nodes.Get (3));
  clientApps4.Start (Seconds (1.3));
  clientApps4.Stop (Seconds (10.0));


  outputFile.open ("ring_data.csv");
  outputFile << "Source,Destination,PacketSize,UID,ArrivalTime" << std::endl;

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> recvSink2 = Socket::CreateSocket (nodes.Get (2), tid);
  InetSocketAddress local2 = InetSocketAddress (Ipv4Address::GetAny (), 9);
  recvSink2->Bind (local2);
  recvSink2->SetRecvCallback (MakeCallback (&ReceivePacket));

    Ptr<Socket> recvSink3 = Socket::CreateSocket (nodes.Get (3), tid);
  InetSocketAddress local3 = InetSocketAddress (Ipv4Address::GetAny (), 9);
  recvSink3->Bind (local3);
  recvSink3->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> recvSink4 = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress local4 = InetSocketAddress (Ipv4Address::GetAny (), 9);
  recvSink4->Bind (local4);
  recvSink4->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  outputFile.close ();

  return 0;
}