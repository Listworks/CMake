/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$


  Copyright (c) 2000 National Library of Medicine
  All rights reserved.

  See COPYRIGHT.txt for copyright details.

=========================================================================*/
#ifndef cmLibraryRule_h
#define cmLibraryRule_h

#include "cmStandardIncludes.h"
#include "cmRuleMaker.h"


/** \class cmLibraryRule
 * \brief Specify a name for a library.
 *
 * cmLibraryRule is used to specify the name of a library to be
 * generated by the build process.
 */
class cmLibraryRule : public cmRuleMaker
{
public:
  /**
   * This is a virtual constructor for the rule.
   */
  virtual cmRuleMaker* Clone() 
    {
    return new cmLibraryRule;
    }

  /**
   * This is called when the rule is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool Invoke(std::vector<std::string>& args);

  /**
   * This is called at the end after all the information
   * specified by the rules is accumulated.
   */
  virtual void FinalPass() { }
  
  /**
   * The name of the rule as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "LIBRARY";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Set a name for a library.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "LIBRARY(libraryname)";
    }
};



#endif
